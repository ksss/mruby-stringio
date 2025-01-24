MRuby::Gem::Specification.new('mruby-stringio') do |spec|
  spec.license = 'MIT'
  spec.author  = 'ksss <co000ri@gmail.com>'
  spec.summary = 'StringIO class'

  spec.add_dependency('mruby-enumerator', core: 'mruby-enumerator')
end
