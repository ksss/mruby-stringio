/*
original is https://github.com/ruby/ruby/blob/trunk/ext/stringio/stringio.c
*/

#include <string.h>
#include <errno.h>
#include "mruby.h"
#include "mruby/string.h"
#include "mruby/variable.h"
#include "mruby/error.h"
#include "mruby/data.h"
#include "mruby/class.h"

#define FMODE_READABLE              0x0001
#define FMODE_WRITABLE              0x0002
#define FMODE_READWRITE             (FMODE_READABLE|FMODE_WRITABLE)
#define FMODE_BINMODE               0x0004
#define FMODE_APPEND                0x0040

#define stringio_iv_get(name) mrb_iv_get(mrb, self, mrb_intern_lit(mrb, name))
#define E_IOERROR (mrb_class_get(mrb, "IOError"))
#define StringIO(self) get_strio(mrb, self)

// For compatibility before https://github.com/mruby/mruby/pull/3340
#ifndef MRB_FROZEN_P
#define MRB_FROZEN_P(o) RSTR_FROZEN_P(o)
#endif

struct StringIO {
  mrb_int pos;
  mrb_int lineno;
  int count;
};

static struct StringIO *
stringio_alloc(mrb_state *mrb)
{
  struct StringIO *ptr = (struct StringIO *)mrb_malloc(mrb, sizeof(struct StringIO));
  ptr->pos = 0;
  ptr->lineno = 0;
  ptr->count = 1;
  return ptr;
}

static void
stringio_free(mrb_state *mrb, void *p) {
  struct StringIO *ptr = ((struct StringIO *)p);
  if (--ptr->count <= 0) {
    mrb_free(mrb, p);
  }
}

static const struct mrb_data_type mrb_stringio_type = { "StringIO", stringio_free };

static struct StringIO *
get_strio(mrb_state *mrb, mrb_value self)
{
  struct StringIO *ptr = mrb_data_get_ptr(mrb, self, &mrb_stringio_type);
  if (!ptr) mrb_raise(mrb, E_IOERROR, "uninitialized stream");
  return ptr;
}

static void
check_modifiable(mrb_state *mrb, mrb_value self)
{
  mrb_value string = stringio_iv_get("@string");
  if (MRB_FROZEN_P(mrb_str_ptr(string))) {
    mrb_raise(mrb, E_IOERROR, "not modifiable string");
  }
}

static void
mrb_syserr_fail(mrb_state *mrb, mrb_int no, const char *mesg) {
  struct RClass *sce;
  if (mrb_class_defined(mrb, "SystemCallError")) {
    sce = mrb_class_get(mrb, "SystemCallError");
    if (mesg) {
      mrb_funcall(mrb, mrb_obj_value(sce), "_sys_fail", 2, mrb_fixnum_value(no), mrb_str_new_cstr(mrb, mesg));
    } else {
      mrb_funcall(mrb, mrb_obj_value(sce), "_sys_fail", 1, mrb_fixnum_value(no));
    }
  } else {
    mrb_raise(mrb, E_RUNTIME_ERROR, mesg);
  }
}

/* Boyer-Moore search: copied from https://github.com/ruby/ruby/ext/stringio/stringio.c */
static void
bm_init_skip(long *skip, const char *pat, long m)
{
  int c;

  for (c = 0; c < (1 << CHAR_BIT); c++) {
    skip[c] = m;
  }
  while (--m) {
    skip[(unsigned char)*pat++] = m;
  }
}

static long
bm_search(const char *little, long llen, const char *big, long blen, const long *skip)
{
  long i, j, k;

  i = llen - 1;
  while (i < blen) {
    k = i;
    j = llen - 1;
    while (j >= 0 && big[k] == little[j]) {
      k--;
      j--;
    }
    if (j < 0) return k + 1;
    i += skip[(unsigned char)big[i]];
  }
  return -1;
}

static mrb_int
modestr_fmode(mrb_state *mrb, const char *modestr)
{
  mrb_int fmode = 0;
  const char *m = modestr;

  switch (*m++) {
    case 'r':
      fmode |= FMODE_READABLE;
      break;
    case 'w':
      fmode |= FMODE_WRITABLE;
      break;
    case 'a':
      fmode |= FMODE_WRITABLE | FMODE_APPEND;
      break;
    default:
      goto error;
  }

  while (*m) {
    switch (*m++) {
      case '+':
        fmode |= FMODE_READWRITE;
        break;
      default:
        goto error;
    }
  }
  return fmode;

error:
  mrb_raisef(mrb, E_ARGUMENT_ERROR, "invalid access mode %S", mrb_str_new_static(mrb, modestr, strlen(modestr)));
  return -1;
}

static mrb_value
strio_substr(mrb_state *mrb, mrb_value self, long pos, long len)
{
  mrb_value str = stringio_iv_get("@string");
  long rlen = RSTRING_LEN(str) - pos;

  if (len > rlen) len = rlen;
  if (len < 0) len = 0;
  if (len == 0) return mrb_str_new(mrb, 0, 0);
  return mrb_str_new(mrb, RSTRING_PTR(str)+pos, len);
}

static void
strio_extend(mrb_state *mrb, mrb_value self, long pos, long len)
{
  long olen;
  mrb_value string = stringio_iv_get("@string");

  olen = RSTRING_LEN(string);
  if (pos + len > olen) {
    mrb_str_resize(mrb, string, pos + len);
    if (pos > olen)
      memset(RSTRING_PTR(string) + olen, 0, sizeof(char) * (pos - olen));
  } else {
    mrb_str_modify(mrb, mrb_str_ptr(string));
  }
}

static void
strio_init(mrb_state *mrb, mrb_value self, mrb_int argc, mrb_value *argv)
{
  mrb_int flags;
  struct StringIO *ptr;
  mrb_value string = mrb_nil_value();
  mrb_value mode = mrb_nil_value();
  switch (argc) {
    case 0:
      break;
    case 1:
      string = argv[0];
      break;
    case 2:
      string = argv[0];
      mode = argv[1];
      break;
    default:
      mrb_raisef(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (given %S, expected 1..2)", argc);
      break;
  }
  if (mrb_nil_p(string)) {
    string = mrb_str_new(mrb, 0, 0);
  }
  if (mrb_nil_p(mode)) {
    flags = MRB_FROZEN_P(mrb_str_ptr(string)) ? FMODE_READABLE : FMODE_READWRITE;
  } else {
    flags = modestr_fmode(mrb, RSTRING_PTR(mrb_string_type(mrb, mode)));
  }

  if (argc == 2 && (flags & FMODE_WRITABLE) && MRB_FROZEN_P(mrb_str_ptr(string))) {
    mrb_syserr_fail(mrb, EACCES, 0);
  }

  ptr = (struct StringIO*)DATA_PTR(self);
  if (ptr) {
    /* reopen */
    mrb_funcall(mrb, mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@string")), "replace", 1, string);
  } else {
    /* initialize */
    ptr = stringio_alloc(mrb);
    mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@string"), string);
  }
  ptr->lineno = 0;
  ptr->pos = 0;
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@flags"), mrb_fixnum_value(flags));
  mrb_data_init(self, ptr, &mrb_stringio_type);
}

static mrb_value
stringio_initialize(mrb_state *mrb, mrb_value self)
{
  mrb_value string = mrb_nil_value();
  mrb_value mode = mrb_nil_value();
  mrb_int argc;
  mrb_value argv[2];

  argc = mrb_get_args(mrb, "|SS", &string, &mode);
  argv[0] = string;
  argv[1] = mode;
  strio_init(mrb, self, argc, argv);
  return self;
}

static mrb_value
stringio_copy(mrb_state *mrb, mrb_value copy, mrb_value orig)
{
  struct StringIO *ptr;
  mrb_value flags;
  mrb_value string;

  orig = mrb_convert_type(mrb, orig, MRB_TT_DATA, "StringIO", "to_strio");
  if (&copy == &orig) return copy;
  ptr = StringIO(orig);
  if (mrb_data_check_get_ptr(mrb, copy, &mrb_stringio_type)) {
    stringio_free(mrb, DATA_PTR(copy));
  }
  mrb_data_init(copy, ptr, &mrb_stringio_type);

  string = mrb_iv_get(mrb, orig, mrb_intern_lit(mrb, "@string"));
  mrb_iv_set(mrb, copy, mrb_intern_lit(mrb, "@string"), string);

  flags = mrb_iv_get(mrb, orig, mrb_intern_lit(mrb, "@flags"));
  mrb_iv_set(mrb, copy, mrb_intern_lit(mrb, "@flags"), flags);

  ++ptr->count;
  return copy;
}

static mrb_value
stringio_initialize_copy(mrb_state *mrb, mrb_value copy)
{
  mrb_value orig;

  mrb_get_args(mrb, "o", &orig);
  return stringio_copy(mrb, copy, orig);
}

static mrb_value
stringio_get_lineno(mrb_state *mrb, mrb_value self)
{
  struct StringIO *ptr = StringIO(self);
  return mrb_fixnum_value(ptr->lineno);
}

static mrb_value
stringio_set_lineno(mrb_state *mrb, mrb_value self)
{
  struct StringIO *ptr = StringIO(self);
  mrb_int lineno = 0;
  mrb_get_args(mrb, "i", &lineno);
  ptr->lineno = lineno;
  return mrb_fixnum_value(lineno);
}

static mrb_value
stringio_get_pos(mrb_state *mrb, mrb_value self)
{
  struct StringIO *ptr = StringIO(self);
  return mrb_fixnum_value(ptr->pos);
}

static mrb_value
stringio_set_pos(mrb_state *mrb, mrb_value self)
{
  struct StringIO *ptr = StringIO(self);
  mrb_int pos = 0;

  mrb_get_args(mrb, "i", &pos);
  if (pos < 0) {
    mrb_syserr_fail(mrb, EINVAL, 0);
  }
  ptr->pos = pos;
  return mrb_fixnum_value(pos);
}

static mrb_value
stringio_rewind(mrb_state *mrb, mrb_value self)
{
  struct StringIO *ptr = StringIO(self);
  ptr->pos = 0;
  ptr->lineno = 0;
  return mrb_fixnum_value(0);
}

static mrb_value
stringio_closed_p(mrb_state *mrb, mrb_value self)
{
  mrb_int flags;

  StringIO(self);
  flags = mrb_fixnum(stringio_iv_get("@flags"));
  return ((flags & FMODE_READWRITE) == 0) ? mrb_true_value() : mrb_false_value();
}

static mrb_value
stringio_close(mrb_state *mrb, mrb_value self)
{
  mrb_int flags;

  StringIO(self);
  flags = mrb_fixnum(stringio_iv_get("@flags"));
  if ((flags & FMODE_READWRITE) == 0)
    mrb_raise(mrb, E_IOERROR, "closed stream");
  flags &= ~FMODE_READWRITE;
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@flags"), mrb_fixnum_value(flags));
  return mrb_nil_value();
}

static mrb_value
stringio_read(mrb_state *mrb, mrb_value self)
{
  mrb_int argc;
  mrb_int clen;
  struct StringIO *ptr = StringIO(self);
  mrb_value rlen = mrb_nil_value();
  mrb_value rstr = mrb_nil_value();
  mrb_value string = stringio_iv_get("@string");
  mrb_int flags = mrb_fixnum(stringio_iv_get("@flags"));

  if ((flags & FMODE_READABLE) != FMODE_READABLE)
    mrb_raise(mrb, E_IOERROR, "not opened for reading");

  argc = mrb_get_args(mrb, "|oo", &rlen, &rstr);
  switch (argc) {
    case 2:
      if (!mrb_nil_p(rstr)) {
        mrb_str_modify(mrb, mrb_str_ptr(rstr));
      }
      /* fall through */
    case 1:
      if (!mrb_nil_p(rlen)) {
        clen = mrb_fixnum(rlen);
        if (clen < 0) {
          mrb_raisef(mrb, E_ARGUMENT_ERROR, "negative length %S given", rlen);
        }
        if (clen > 0 && ptr->pos >= RSTRING_LEN(string)) {
          if (!mrb_nil_p(rstr)) mrb_str_resize(mrb, rstr, 0);
          return mrb_nil_value();
        }
        break;
      }
      /* fall through */
    case 0:
      clen = RSTRING_LEN(string);
      if (clen <= ptr->pos) {
        if (mrb_nil_p(rstr)) {
          rstr = mrb_str_new(mrb, 0, 0);
        } else {
          mrb_str_resize(mrb, rstr, 0);
        }
        return rstr;
      } else {
        clen -= ptr->pos;
      }
      break;
    default:
      mrb_raisef(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (given %S, expected 0..2)", mrb_fixnum_value(argc));
      break;
  }
  if (mrb_nil_p(rstr)) {
    rstr = strio_substr(mrb, self, ptr->pos, clen);
  } else {
    long rest = RSTRING_LEN(string) - ptr->pos;
    if (clen > rest) clen = rest;
    mrb_str_resize(mrb, rstr, clen);
    memcpy(RSTRING_PTR(rstr), RSTRING_PTR(string) + ptr->pos, sizeof(char) * clen);
  }
  ptr->pos += RSTRING_LEN(rstr);
  return rstr;
}

static mrb_value
stringio_write(mrb_state *mrb, mrb_value self)
{
  struct StringIO *ptr = StringIO(self);
  mrb_int len, olen;
  mrb_value str = mrb_nil_value();
  mrb_value string = stringio_iv_get("@string");
  mrb_int flags = mrb_fixnum(stringio_iv_get("@flags"));

  if ((flags & FMODE_WRITABLE) != FMODE_WRITABLE)
    mrb_raise(mrb, E_IOERROR, "not opened for writing");

  mrb_get_args(mrb, "o", &str);

  if (!mrb_string_p(str))
    str = mrb_obj_as_string(mrb, str);

  len = RSTRING_LEN(str);
  if (len == 0)
    return mrb_fixnum_value(0);
  check_modifiable(mrb, self);
  olen = RSTRING_LEN(string);
  if (flags & FMODE_APPEND) {
    ptr->pos = olen;
  }
  if (ptr->pos == olen) {
    mrb_str_append(mrb, string, str);
  } else {
    strio_extend(mrb, self, ptr->pos, len);
    memmove(RSTRING_PTR(string) + ptr->pos, RSTRING_PTR(str), len);
  }
  ptr->pos += len;
  return mrb_fixnum_value(len);
}

static mrb_value
stringio_getc(mrb_state *mrb, mrb_value self)
{
  struct StringIO *ptr = StringIO(self);
  mrb_value string = stringio_iv_get("@string");
  mrb_value ret;
  mrb_int flags = mrb_fixnum(stringio_iv_get("@flags"));

  if ((flags & FMODE_READABLE) == 0)
    mrb_raise(mrb, E_IOERROR, "not opened for reading");

  if (ptr->pos >= RSTRING_LEN(string)) {
    return mrb_nil_value();
  }

  ret = mrb_str_new(mrb, RSTRING_PTR(string) + ptr->pos, 1);
  ptr->pos += 1;
  return ret;
}

static mrb_value
stringio_gets(mrb_state *mrb, mrb_value self)
{
  struct StringIO *ptr = StringIO(self);
  mrb_value string = stringio_iv_get("@string");
  mrb_int flags = mrb_fixnum(stringio_iv_get("@flags"));
  mrb_int argc;
  mrb_value *argv;
  mrb_value mrb_rs = mrb_str_new_lit(mrb, "\n");
  mrb_value str = mrb_nil_value();
  mrb_value lim = mrb_nil_value();
  const char *s, *e, *p;
  long n, limit = 0;

  if ((flags & FMODE_READABLE) != FMODE_READABLE)
    mrb_raise(mrb, E_IOERROR, "not opened for reading");

  mrb_get_args(mrb, "*", &argv, &argc);
  switch (argc) {
    case 0:
      str = mrb_rs;
      break;
    case 1:
      str = argv[0];
      if (!mrb_nil_p(str) && !mrb_string_p(str)) {
        mrb_value tmp = mrb_check_string_type(mrb, str);
        if (mrb_nil_p(tmp)) {
          limit = mrb_fixnum(str);
          if (limit == 0) return mrb_str_new(mrb, 0, 0);
          str = mrb_rs;
        } else {
          str = tmp;
        }
      }
      break;
    case 2:
      str = argv[0];
      lim = argv[1];
      if (!mrb_nil_p(str)) str = mrb_string_type(mrb, str);
      if (!mrb_nil_p(lim)) limit = mrb_fixnum(lim);
      break;
  }

  if (ptr->pos >= (n = RSTRING_LEN(string))) {
    return mrb_nil_value();
  }
  s = RSTRING_PTR(string);
  e = s + RSTRING_LEN(string);
  s += ptr->pos;
  if (limit > 0 && s + limit < e) {
    e = s + limit;
  }
  if (mrb_nil_p(str)) {
    str = strio_substr(mrb, self, ptr->pos, e - s);
  }
  else if ((n = RSTRING_LEN(str)) == 0) {
    p = s;
    while (*p == '\n') {
      if (++p == e) {
        return mrb_nil_value();
      }
    }
    s = p;
    while ((p = memchr(p, '\n', e - p)) && (p != e)) {
      if (*++p == '\n') {
        e = p + 1;
        break;
      }
    }
    str = strio_substr(mrb, self, s - RSTRING_PTR(string), e - s);
  }
  else if (n == 1) {
    if ((p = memchr(s, RSTRING_PTR(str)[0], e - s)) != 0) {
      e = p + 1;
    }
    str = strio_substr(mrb, self, ptr->pos, e - s);
  }
  else {
    if (n < e - s) {
      if (e - s < 1024) {
        for (p = s; p + n <= e; ++p) {
          if (memcmp(p, RSTRING_PTR(str), sizeof(char) * n) == 0) {
            e = p + n;
            break;
          }
        }
      } else {
        long skip[1 << CHAR_BIT], pos;
        p = RSTRING_PTR(str);
        bm_init_skip(skip, p, n);
        if ((pos = bm_search(p, n, s, e - s, skip)) >= 0) {
          e = s + pos + n;
        }
      }
    }
    str = strio_substr(mrb, self, ptr->pos, e - s);
  }
  ptr->pos = e - RSTRING_PTR(string);
  ptr->lineno++;
  return str;
}

static mrb_value
stringio_seek(mrb_state *mrb, mrb_value self)
{
  struct StringIO *ptr = StringIO(self);
  mrb_value whence = mrb_nil_value();
  mrb_int offset = 0;
  mrb_value string = stringio_iv_get("@string");
  mrb_int flags = mrb_fixnum(stringio_iv_get("@flags"));

  mrb_get_args(mrb, "i|o", &offset, &whence);
  if ((flags & FMODE_READWRITE) == 0) {
    mrb_raise(mrb, E_IOERROR, "closed stream");
  }
  switch (mrb_nil_p(whence) ? 0 : mrb_fixnum(whence)) {
    case SEEK_SET:
      break;
    case SEEK_CUR:
      offset += ptr->pos;
      break;
    case SEEK_END:
      offset += RSTRING_LEN(string);
      break;
    default:
      mrb_syserr_fail(mrb, EINVAL, "invalid whence");
  }
  if (offset < 0) {
    mrb_syserr_fail(mrb, EINVAL, 0);
  }
  ptr->pos = offset;
  return mrb_fixnum_value(0);
}

static mrb_value
stringio_size(mrb_state *mrb, mrb_value self)
{
  mrb_value string = stringio_iv_get("@string");
  if (mrb_nil_p(string)) {
    mrb_raise(mrb, E_IOERROR, "not opened");
  }
  return mrb_fixnum_value(RSTRING_LEN(string));
}

static mrb_value
stringio_eof_p(mrb_state *mrb, mrb_value self)
{
  mrb_value string = stringio_iv_get("@string");
  struct StringIO *ptr = StringIO(self);
  return (RSTRING_LEN(string) <= ptr->pos) ? mrb_true_value() : mrb_false_value();
}

static mrb_value
stringio_reopen(mrb_state *mrb, mrb_value self)
{
  mrb_int argc;
  mrb_value *argv;

  mrb_get_args(mrb, "*", &argv, &argc);
  if (argc == 1 && !(mrb_type(*argv) == MRB_TT_STRING)) {
    return stringio_copy(mrb, self, *argv);
  }
  strio_init(mrb, self, argc, argv);
  return self;
}

void
mrb_mruby_stringio_gem_init(mrb_state* mrb)
{
  struct RClass *stringio = mrb_define_class(mrb, "StringIO", mrb->object_class);
  MRB_SET_INSTANCE_TT(stringio, MRB_TT_DATA);
  mrb_define_method(mrb, stringio, "initialize", stringio_initialize, MRB_ARGS_ANY());
  mrb_define_method(mrb, stringio, "initialize_copy", stringio_initialize_copy, MRB_ARGS_ANY());
  mrb_define_method(mrb, stringio, "lineno", stringio_get_lineno, MRB_ARGS_NONE());
  mrb_define_method(mrb, stringio, "lineno=", stringio_set_lineno, MRB_ARGS_NONE());
  mrb_define_method(mrb, stringio, "pos", stringio_get_pos, MRB_ARGS_NONE());
  mrb_define_method(mrb, stringio, "pos=", stringio_set_pos, MRB_ARGS_NONE());
  mrb_define_method(mrb, stringio, "rewind", stringio_rewind, MRB_ARGS_NONE());
  mrb_define_method(mrb, stringio, "closed?", stringio_closed_p, MRB_ARGS_NONE());
  mrb_define_method(mrb, stringio, "close", stringio_close, MRB_ARGS_NONE());
  mrb_define_method(mrb, stringio, "read", stringio_read, MRB_ARGS_ANY());
  mrb_define_method(mrb, stringio, "write", stringio_write, MRB_ARGS_REQ(1));
  mrb_define_alias(mrb, stringio, "syswrite", "write");
  mrb_define_method(mrb, stringio, "getc", stringio_getc, MRB_ARGS_ANY());
  mrb_define_method(mrb, stringio, "gets", stringio_gets, MRB_ARGS_ANY());
  mrb_define_method(mrb, stringio, "seek", stringio_seek, MRB_ARGS_NONE());
  mrb_define_method(mrb, stringio, "size", stringio_size, MRB_ARGS_NONE());
  mrb_define_alias(mrb, stringio, "length", "size");
  mrb_define_method(mrb, stringio, "eof?", stringio_eof_p, MRB_ARGS_NONE());
  mrb_define_alias(mrb, stringio, "eof", "eof?");
  mrb_define_method(mrb, stringio, "reopen", stringio_reopen, MRB_ARGS_NONE());

  struct RClass *io = mrb_define_class(mrb, "IO", mrb->object_class);
  /* Set I/O position from the beginning */
  mrb_define_const(mrb, io, "SEEK_SET", mrb_fixnum_value(SEEK_SET));
  /* Set I/O position from the current position */
  mrb_define_const(mrb, io, "SEEK_CUR", mrb_fixnum_value(SEEK_CUR));
  /* Set I/O position from the end */
  mrb_define_const(mrb, io, "SEEK_END", mrb_fixnum_value(SEEK_END));

  struct RClass *io_error = mrb_define_class(mrb, "IOError", mrb->eStandardError_class);
  mrb_define_class(mrb, "EOFError", io_error);
}

void
mrb_mruby_stringio_gem_final(mrb_state* mrb)
{
}
