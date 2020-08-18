assert 'StringIO#initialize' do
  assert_kind_of StringIO, StringIO.new
  assert_kind_of StringIO, StringIO.new('str')
  assert_kind_of StringIO, StringIO.new('str', 'r+')
  assert_raise(ArgumentError) { StringIO.new('', 'x') }
  assert_raise(ArgumentError) { StringIO.new('', 'r+x') }
  assert_raise(TypeError) { StringIO.new(nil) }
  assert_raise(TypeError) { StringIO.new('', nil) }

  frozen = ''.freeze
  assert_kind_of StringIO, StringIO.new(frozen)
  assert_raise(RuntimeError) { StringIO.new(frozen, 'r+') }

  o = Object.new
  def o.to_str
    nil
  end
  assert_raise(TypeError) { StringIO.new(o) }
  # def o.to_str
  #   ''
  # end
  # assert_kind_of StringIO, StringIO.new(o)
end

assert 'StringIO#dup' do
  begin
    f1 = StringIO.new("1234")
    assert_equal("1", f1.getc)
    f2 = f1.dup
    assert_equal("2", f2.getc)
    assert_equal("3", f1.getc)
    assert_equal("4", f2.getc)
    assert_equal(nil, f1.getc)
    assert_equal(true, f2.eof?)
    f1.close
    assert_equal(false, f2.closed?)
  ensure
    f1.close unless f1.closed?
    f2.close unless f2.closed?
  end
end

assert 'StringIO#pos' do
  f = StringIO.new("foo\nbar\nbaz\n")
  assert_equal([0, "foo\n"], [f.pos, f.gets])
  assert_equal([4, "bar\n"], [f.pos, f.gets])
  assert_raise(RuntimeError) { f.pos = -1 }
  f.pos = 1
  assert_equal([1, "oo\n"], [f.pos, f.gets])
  assert_equal([4, "bar\n"], [f.pos, f.gets])
  assert_equal([8, "baz\n"], [f.pos, f.gets])
  assert_equal([12, nil], [f.pos, f.gets])
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

assert 'StringIO#size' do
  strio = StringIO.new("1234")
  assert_equal(4, strio.size)
end

assert 'StringIO#each' do
  f = StringIO.new("foo\nbar\nbaz\n")
  assert_equal(["foo\n", "bar\n", "baz\n"], f.each.to_a)
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

assert 'StringIO#write_nonblock' do
  assert_equal 1, StringIO.new.write_nonblock("a")
  assert_equal 1, StringIO.new.write_nonblock("a", exception: true)
  assert_equal 1, StringIO.new.write_nonblock("a", nil)
end

assert 'StringiO#print' do
  strio = StringIO.new("test")
  assert_nil strio.print("b")
  assert_equal "best", strio.string
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
  assert_equal "test", strio.read

  strio.rewind
  assert_equal "test", strio.read(strio.size)

  strio.rewind
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

assert 'StringIO#read_nonblock' do
  f = StringIO.new("\u3042\u3044")
  assert_raise(ArgumentError) { f.read_nonblock(-1) }
  assert_raise(ArgumentError) { f.read_nonblock(1, 2, 3) }
  assert_equal("\u3042\u3044", f.read_nonblock(100))
  assert_raise(EOFError) { f.read_nonblock(10) }
  f.rewind
  assert_equal("\u3042\u3044", f.read_nonblock(f.size))
end

assert 'read_nonblock_no_exceptions' do
  f = StringIO.new("\u3042\u3044")
  assert_raise(ArgumentError) { f.read_nonblock(-1, exception: false) }
  assert_raise(ArgumentError) { f.read_nonblock(1, 2, 3, exception: false) }
  assert_raise(ArgumentError) { f.read_nonblock }
  assert_equal("\u3042\u3044", f.read_nonblock(100, exception: false))
  assert_raise(EOFError) { f.read_nonblock(10, typo: false) }
  assert_equal(nil, f.read_nonblock(10, exception: false))
  f.rewind
  assert_equal("\u3042\u3044", f.read_nonblock(f.size))
  f.rewind
  # not empty buffer
  s = '0123456789'
  assert_equal("\u3042\u3044", f.read_nonblock(f.size, s))
end

assert 'StringIO#sysread' do
  strio = StringIO.new("test")
  assert_equal "tes", strio.sysread(3)
  assert_equal "t", strio.sysread(10)
  assert_raise(EOFError){ strio.sysread(10) }
end

assert 'StringIO#getc' do
  strio = StringIO.new("abc")
  assert_equal "a", strio.getc
  assert_equal "b", strio.getc
  assert_equal "c", strio.getc
  assert_equal nil, strio.getc
end

assert 'StringIO#gets' do
  io = StringIO.new("this>is>an>example")
  assert_equal "this>", io.gets(">")
  assert_equal "is>", io.gets(">")
  assert_equal "an>", io.gets(">")
  assert_equal "example", io.gets(">")
  assert_equal nil, io.gets(">")

  io = StringIO.new("this>>is>>an>>example")
  assert_equal "this>>", io.gets(">>")
  assert_equal "is>>", io.gets(">>")
  assert_equal "an>>", io.gets(">>")
  assert_equal "example", io.gets(">>")
  assert_equal nil, io.gets(">>")
end

assert 'gets pos and lineno' do
  io = StringIO.new("this is\nan example\nfor StringIO#gets")
  io.gets
  assert_equal 8, io.pos
  assert_equal 1, io.lineno
  io.gets
  assert_equal 19, io.pos
  assert_equal 2, io.lineno
  io.gets
  assert_equal 36, io.pos
  assert_equal 3, io.lineno
end

assert 'gets1' do
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

assert 'overwrite' do
  stringio = StringIO.new
  responses = ['', 'just another ruby', 'hacker']
  responses.each do |resp|
    stringio.puts(resp)
    stringio.rewind
  end
  assert_equal("hacker\nother ruby\n", stringio.string)
end

assert 'StringIO#seek' do
  begin
    f = StringIO.new("1234")
    assert_raise(RuntimeError) { f.seek(-1) }
    f.seek(-1, 2)
    assert_equal("4", f.getc)
    assert_raise(RuntimeError) { f.seek(0, 3) }
    f.close
    assert_raise(IOError) { f.seek(1) }
  ensure
    f.close unless f.closed?
  end
end

assert 'gets2' do
  f = StringIO.new("foo\nbar\nbaz\n")
  assert_equal("fo", f.gets(2))
  # o = Object.new
  # def o.to_str; "z"; end
  # assert_equal("o\nbar\nbaz", f.gets(o))

  f = StringIO.new("foo\nbar\nbaz\n")
  assert_equal("foo\nbar\nbaz", f.gets("az"))
  f = StringIO.new("a" * 10000 + "zz!")
  assert_equal("a" * 10000 + "zz", f.gets("zz"))
  f = StringIO.new("a" * 10000 + "zz!")
  assert_equal("a" * 10000 + "zz!", f.gets("zzz"))

  assert_equal("a", StringIO.new("a").gets(1))
  assert_equal("a", StringIO.new("a").gets(nil, 1))
end

assert 'seek_beyond_eof' do
  io = StringIO.new
  n = 10
  io.seek(n)
  io.print "last"
  assert_equal("\0" * n + "last", io.string)
end

assert 'eof?' do
  io = StringIO.new("test")
  assert_false io.eof?
  assert_false io.eof
  io.seek(3)
  assert_false io.eof?
  assert_false io.eof
  io.seek(4)
  assert_true io.eof?
  assert_true io.eof
end

assert 'StringIO#tty?' do
  assert_false StringIO.new.tty?
  assert_false StringIO.new.isatty
end

assert 'StringIO#sync' do
  s = StringIO.new
  assert_true s.sync
  assert_false s.sync = false
  assert_true s.sync
  assert_true s.sync = true
end

assert 'StringIO#fsync' do
  assert_equal 0, StringIO.new.fsync
end

assert 'StringIO#flush' do
  s = StringIO.new
  assert_equal s, s.flush
end

assert 'StringIO#reopen' do
  begin
    f = StringIO.new("foo\nbar\nbaz\n")
    assert_equal("foo\n", f.gets)
    f.reopen("qux\nquux\nquuux\n")
    assert_equal("qux\n", f.gets)

    f2 = StringIO.new("")
    f2.reopen(f)
    assert_equal("quux\n", f2.gets)
  ensure
    f.close unless f.closed?
  end
end

assert 'reopen with dup' do
  f = StringIO.new("foo\nbar\nbaz\n")
  assert_equal("foo\n", f.gets)
  f.dup.reopen("qux\nquux\nquuux\n")
  assert_equal("qux\n", f.gets)
end

assert 'StringIO#readchar' do
  f = StringIO.new('1234')
  a = ''
  assert_equal '1', a.replace(a + f.readchar)
  assert_equal '12', a.replace(a + f.readchar)
  assert_equal '123', a.replace(a + f.readchar)
  assert_equal '1234', a.replace(a + f.readchar)
  assert_raise(EOFError) { f.readchar }
end

assert 'StringIO#puts' do
  f = StringIO.new
  f.puts(1, 2, 3, 4)
  assert_equal("1\n2\n3\n4\n", f.string)

  f = StringIO.new
  f.puts('')
  assert_equal("\n", f.string)

  f = StringIO.new
  f.puts
  assert_equal("\n", f.string)

  f = StringIO.new('', 'r')
  assert_raise(IOError) { f.puts }
end

assert 'StringIO#ungetc' do
  s = "1234"
  f = StringIO.new(s, "r")
  f.ungetc("x")
  assert_equal("x", f.getc)
  assert_equal("1", f.getc)

  s = "1234"
  f = StringIO.new(s, "r")
  assert_equal("1", f.getc)
  f.ungetc("y")
  assert_equal("y", f.getc)
  assert_equal("2", f.getc)

  s = "12"
  f = StringIO.new(s)
  f.pos = 1
  f.ungetc('aaa')
  assert_equal("aaa2", f.string)

  s = "1"
  f = StringIO.new(s)
  f.pos = 1
  assert_raise(RuntimeError) { f.ungetc('aaa') }

  s = StringIO.new("".freeze)
  assert_raise(IOError) {s.ungetc("x")}

  s = StringIO.new("", "w")
  assert_raise(IOError) {s.ungetc("x")}

  s = StringIO.new("").freeze
  assert_raise(FrozenError) {s.ungetc("x")}
end
