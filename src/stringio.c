#include <string.h>
#include "mruby.h"
#include "mruby/string.h"
#include "mruby/variable.h"

#define FMODE_READABLE              0x0001
#define FMODE_WRITABLE              0x0002
#define FMODE_READWRITE             (FMODE_READABLE|FMODE_WRITABLE)
#define FMODE_BINMODE               0x0004
#define FMODE_APPEND                0x0040

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
    flags = mrb_fixnum_value(FMODE_READABLE | FMODE_READWRITE);
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
}

void
mrb_mruby_stringio_gem_final(mrb_state* mrb)
{
}
