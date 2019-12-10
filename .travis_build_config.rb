MRuby::Build.new do |conf|
  toolchain :gcc
  conf.enable_test

  conf.gem '../mruby-stringio'
end
