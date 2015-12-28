#include <string.h>
#include "mruby.h"
#include "mruby/string.h"
#include "mruby/variable.h"

#define FMODE_READABLE              0x0001
#define FMODE_WRITABLE              0x0002
#define FMODE_READWRITE             (FMODE_READABLE|FMODE_WRITABLE)
#define FMODE_BINMODE               0x0004
#define FMODE_APPEND                0x0040

#define stringio_iv_get(name) mrb_iv_get(mrb, self, mrb_intern_lit(mrb, name))
#define E_IOERROR (mrb_class_get(mrb, "IOError"))

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
  mrb_value flags;

  mrb_get_args(mrb, "|SS", &string, &mode);

  if (mrb_nil_p(string)) {
    string = mrb_str_new(mrb, 0, 0);
  }
  if (mrb_nil_p(mode)) {
    flags = mrb_fixnum_value(FMODE_READWRITE);
  } else {
    flags = mrb_fixnum_value(modestr_fmode(mrb, RSTRING_PTR(mode)));
  }
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@string"), string);
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@pos"), mrb_fixnum_value(0));
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@lineno"), mrb_fixnum_value(0));
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@flags"), flags);

  return self;
}

void
mrb_mruby_stringio_gem_init(mrb_state* mrb)
{
  struct RClass *stringio = mrb_define_class(mrb, "StringIO", mrb->object_class);
  mrb_define_method(mrb, stringio, "initialize", stringio_initialize, MRB_ARGS_ANY());
  mrb_define_method(mrb, stringio, "read", stringio_read, MRB_ARGS_ANY());
}

void
mrb_mruby_stringio_gem_final(mrb_state* mrb)
{
}
