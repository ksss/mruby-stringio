assert 'StringIO#initialize' do
  assert_kind_of StringIO, StringIO.new
  assert_kind_of StringIO, StringIO.new("str")
  assert_kind_of StringIO, StringIO.new("str", "r+")
end

assert 'StringIO#string' do
  s = "foo"
  strio = StringIO.new(s, 'w')
  assert_true s.equal?(strio.string)
end

assert 'StringIO#close' do
  strio = StringIO.new
  assert_nil strio.close
  assert_raise(IOError){ strio.close }
end

assert 'StringIO#closed?' do
  strio = StringIO.new
  assert_false strio.closed?
  strio.close
  assert_true strio.closed?
end

assert 'StringIO.open' do
  ret = []
  strio = StringIO.open("foo") do |io|
    ret << io
  end
  assert_kind_of StringIO, strio
  assert_true strio.equal?(ret[0])
  assert_true strio.closed?
  assert_equal nil, strio.string
end

assert 'StringIO#rewind' do
  strio = StringIO.new("test")
  strio.pos = 2

  assert_equal 0, strio.rewind
  assert_equal 0, strio.pos
end

assert 'StringIO#write' do
  begin
    s = ""
    strio = StringIO.new(s, "w")
    assert_equal("", s)
    assert_equal 3, strio.write("foo")
    assert_equal("foo", s)
    assert_nil strio.close
    assert_equal("foo", s)

    strio = StringIO.new(s, "a")
    o = Object.new
    def o.to_s; "baz"; end
    strio.write(o)
    strio.close
    assert_equal("foobaz", s)
    assert_raise(IOError) { strio.write("") }
  ensure
    strio.close unless strio.closed?
  end
end

assert 'StringIO#read' do
  strio = StringIO.new("test")
  assert_equal "test", strio.read

  strio.rewind
  assert_equal "t", strio.read(1)
  assert_equal "es", strio.read(2)
  assert_equal "t", strio.read(3)

  strio.rewind
  assert_raise(ArgumentError) { strio.read(-1) }
  assert_raise(ArgumentError) { strio.read(1, 2, 3) }
  assert_equal "test", strio.read(nil, nil)

  strio.rewind
  s = ""
  strio.read(nil, s)
  assert_equal "test", s

  strio.rewind
  s = "0123456789"
  strio.read(nil, s)
  assert_equal "test", s
end

assert 'StringIO#gets' do
  assert_equal(nil, StringIO.new("").gets)
  assert_equal("\n", StringIO.new("\n").gets)
  assert_equal("a\n", StringIO.new("a\n").gets)
  assert_equal("a\n", StringIO.new("a\nb\n").gets)
  assert_equal("a", StringIO.new("a").gets)
  assert_equal("a\n", StringIO.new("a\nb").gets)
  assert_equal("abc\n", StringIO.new("abc\n\ndef\n").gets)
  assert_equal("abc\n\ndef\n", StringIO.new("abc\n\ndef\n").gets(nil))
  assert_equal("abc\n\n", StringIO.new("abc\n\ndef\n").gets(""))
  assert_raise(TypeError){StringIO.new("").gets(1, 1)}
  assert_nothing_raised {StringIO.new("").gets(nil, nil)}
end

assert 'test_overwrite' do
  stringio = StringIO.new
  responses = ['', 'just another ruby', 'hacker']
  responses.each do |resp|
    stringio.puts(resp)
    stringio.rewind
  end
  assert_equal("hacker\nother ruby\n", stringio.string)
end
