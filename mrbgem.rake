MRuby::Gem::Specification.new('mruby-stringio') do |spec|
  spec.license = 'MIT'
  spec.author  = 'mruby developers'
  spec.summary = 'StringIO class'

  spec.add_dependency('mruby-print', core: 'mruby-print')
  spec.add_dependency('mruby-enumerator', core: 'mruby-enumerator')
  spec.add_dependency('mruby-errno')
end
