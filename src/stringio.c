/*
original is https://github.com/ruby/ruby/blob/trunk/ext/stringio/stringio.c
*/

#include <string.h>
#include <errno.h>
#include "mruby.h"
#include "mruby/string.h"
#include "mruby/variable.h"
#include "mruby/error.h"

#define FMODE_READABLE              0x0001
#define FMODE_WRITABLE              0x0002
#define FMODE_READWRITE             (FMODE_READABLE|FMODE_WRITABLE)
#define FMODE_BINMODE               0x0004
#define FMODE_APPEND                0x0040

#define stringio_iv_get(name) mrb_iv_get(mrb, self, mrb_intern_lit(mrb, name))
#define E_IOERROR (mrb_class_get(mrb, "IOError"))

static void
check_modifiable(mrb_state *mrb, mrb_value self)
{
  mrb_value string = stringio_iv_get("@string");
  if (RSTR_FROZEN_P(mrb_str_ptr(string))) {
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

static mrb_value
stringio_read(mrb_state *mrb, mrb_value self)
{
  mrb_int argc;
  mrb_int clen;
  mrb_int pos = mrb_fixnum(stringio_iv_get("@pos"));
  mrb_value rlen = mrb_nil_value();
  mrb_value rstr = mrb_nil_value();
  mrb_value string = stringio_iv_get("@string");
  mrb_value flags = stringio_iv_get("@flags");

  if ((mrb_fixnum(flags) & FMODE_READABLE) != FMODE_READABLE)
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
        if (clen > 0 && pos >= RSTRING_LEN(string)) {
          if (!mrb_nil_p(rstr)) mrb_str_resize(mrb, rstr, 0);
          return mrb_nil_value();
        }
        break;
      }
      /* fall through */
    case 0:
      clen = RSTRING_LEN(string);
      if (clen <= pos) {
        if (mrb_nil_p(rstr)) {
          rstr = mrb_str_new(mrb, 0, 0);
        } else {
          mrb_str_resize(mrb, rstr, 0);
        }
        return rstr;
      } else {
        clen -= pos;
      }
      break;
    default:
      mrb_raisef(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (given %S, expected 0..2)", mrb_fixnum_value(argc));
      break;
  }
  if (mrb_nil_p(rstr)) {
    rstr = strio_substr(mrb, self, pos, clen);
  } else {
    long rest = RSTRING_LEN(string) - pos;
    if (clen > rest) clen = rest;
    mrb_str_resize(mrb, rstr, clen);
    memcpy(RSTRING_PTR(rstr), RSTRING_PTR(string) + pos, sizeof(char) * clen);
  }
  pos += RSTRING_LEN(rstr);
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@pos"), mrb_fixnum_value(pos));
  return rstr;
}

static mrb_value
stringio_initialize(mrb_state *mrb, mrb_value self)
{
  mrb_value string = mrb_nil_value();
  mrb_value mode = mrb_nil_value();
  mrb_int flags;
  mrb_int argc;

  argc = mrb_get_args(mrb, "|SS", &string, &mode);

  if (mrb_nil_p(string)) {
    string = mrb_str_new(mrb, 0, 0);
  }
  if (mrb_nil_p(mode)) {
    flags = RSTR_FROZEN_P(mrb_str_ptr(string)) ? FMODE_READABLE : FMODE_READWRITE;
  } else {
    flags = modestr_fmode(mrb, RSTRING_PTR(mode));
  }

  if (argc == 2 && (flags & FMODE_WRITABLE) && RSTR_FROZEN_P(mrb_str_ptr(string))) {
    mrb_syserr_fail(mrb, EACCES, 0);
  }
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@string"), string);
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@pos"), mrb_fixnum_value(0));
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@lineno"), mrb_fixnum_value(0));
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@flags"), mrb_fixnum_value(flags));

  return self;
}

static mrb_value
stringio_write(mrb_state *mrb, mrb_value self)
{
  mrb_int len, olen;
  mrb_int pos = mrb_fixnum(stringio_iv_get("@pos"));
  mrb_int flags = mrb_fixnum(stringio_iv_get("@flags"));
  mrb_value str = mrb_nil_value();
  mrb_value string = stringio_iv_get("@string");

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
    pos = olen;
  }
  if (pos == olen) {
    mrb_str_append(mrb, string, str);
  } else {
    strio_extend(mrb, self, pos, len);
    memmove(RSTRING_PTR(string) + pos, RSTRING_PTR(str), len);
  }
  pos += len;

  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@pos"), mrb_fixnum_value(pos));
  return mrb_fixnum_value(len);
}

static mrb_value
stringio_getc(mrb_state *mrb, mrb_value self)
{
  mrb_int pos = mrb_fixnum(stringio_iv_get("@pos"));
  mrb_int flags = mrb_fixnum(stringio_iv_get("@flags"));
  mrb_value string = stringio_iv_get("@string");
  mrb_value ret;

  if ((flags & FMODE_READABLE) == 0)
    mrb_raise(mrb, E_IOERROR, "not opened for reading");

  if (pos >= RSTRING_LEN(string)) {
    return mrb_nil_value();
  }

  ret = mrb_str_new(mrb, RSTRING_PTR(string) + pos, 1);
  pos += 1;
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@pos"), mrb_fixnum_value(pos));
  return ret;
}

static mrb_value
stringio_gets(mrb_state *mrb, mrb_value self)
{
  mrb_int pos = mrb_fixnum(stringio_iv_get("@pos"));
  mrb_int lineno = mrb_fixnum(stringio_iv_get("@lineno"));
  mrb_int flags = mrb_fixnum(stringio_iv_get("@flags"));
  mrb_value string = stringio_iv_get("@string");
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

  if (pos >= (n = RSTRING_LEN(string))) {
    return mrb_nil_value();
  }
  s = RSTRING_PTR(string);
  e = s + RSTRING_LEN(string);
  s += pos;
  if (limit > 0 && s + limit < e) {
    e = s + limit;
  }
  if (mrb_nil_p(str)) {
    str = strio_substr(mrb, self, pos, e - s);
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
    str = strio_substr(mrb, self, pos, e - s);
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
    str = strio_substr(mrb, self, pos, e - s);
  }
  pos = e - RSTRING_PTR(string);
  lineno++;
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@pos"), mrb_fixnum_value(pos));
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@lineno"), mrb_fixnum_value(lineno));
  return str;
}

static mrb_value
stringio_seek(mrb_state *mrb, mrb_value self)
{
  mrb_value whence = mrb_nil_value();
  mrb_int offset = 0;
  mrb_int pos = mrb_fixnum(stringio_iv_get("@pos"));
  mrb_int flags = mrb_fixnum(stringio_iv_get("@flags"));
  mrb_value string = stringio_iv_get("@string");

  mrb_get_args(mrb, "i|o", &offset, &whence);
  if ((flags & FMODE_READWRITE) == 0) {
    mrb_raise(mrb, E_IOERROR, "closed stream");
  }
  switch (mrb_nil_p(whence) ? 0 : mrb_fixnum(whence)) {
    case SEEK_SET:
      break;
    case SEEK_CUR:
      offset += pos;
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
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@pos"), mrb_fixnum_value(offset));
  return mrb_fixnum_value(0);
}


void
mrb_mruby_stringio_gem_init(mrb_state* mrb)
{
  struct RClass *stringio = mrb_define_class(mrb, "StringIO", mrb->object_class);
  mrb_define_method(mrb, stringio, "initialize", stringio_initialize, MRB_ARGS_ANY());
  mrb_define_method(mrb, stringio, "read", stringio_read, MRB_ARGS_ANY());
  mrb_define_method(mrb, stringio, "write", stringio_write, MRB_ARGS_REQ(1));
  mrb_define_alias(mrb, stringio, "syswrite", "write");
  mrb_define_method(mrb, stringio, "getc", stringio_getc, MRB_ARGS_ANY());
  mrb_define_method(mrb, stringio, "gets", stringio_gets, MRB_ARGS_ANY());
  mrb_define_method(mrb, stringio, "seek", stringio_seek, MRB_ARGS_NONE());

  struct RClass *io = mrb_define_class(mrb, "IO", mrb->object_class);
  /* Set I/O position from the beginning */
  mrb_define_const(mrb, io, "SEEK_SET", mrb_fixnum_value(SEEK_SET));
  /* Set I/O position from the current position */
  mrb_define_const(mrb, io, "SEEK_CUR", mrb_fixnum_value(SEEK_CUR));
  /* Set I/O position from the end */
  mrb_define_const(mrb, io, "SEEK_END", mrb_fixnum_value(SEEK_END));
}

void
mrb_mruby_stringio_gem_final(mrb_state* mrb)
{
}
