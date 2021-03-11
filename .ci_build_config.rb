def gem_config(conf)
  conf.gem '../'
end

MRuby::Build.new do |conf|
  toolchain :gcc
  conf.enable_test
  if ENV['DISABLE_PRESYM'] == 'true'
    conf.disable_presym
  end

  gem_config(conf)
end
