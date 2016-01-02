class IOError < StandardError
end
class EOFError < IOError
end

class IO
  SEEK_SET = 0
end

class StringIO
  include Enumerable

  READABLE  = 0x0001
  WRITABLE  = 0x0002
  READWRITE = READABLE | WRITABLE
  BINMODE   = 0x0004
  APPEND    = 0x0040
  CREATE    = 0x0080
  TRUNC     = 0x0800
  DEFAULT_RS = "\n"

  class << self
    def open(string = "", mode = "r+", &block)
      instance = new string, mode
      block.call instance if block
      instance
    ensure
      instance.string = nil
      instance.close unless instance.closed?
    end
  end

  attr_accessor :string, :pos, :lineno

  def rewind
    @pos = 0
    @lineno = 0
    0
  end

  def closed?
    (@flags & READWRITE) == 0
  end

  def close
    raise IOError, "closed stream" unless !closed?
    @flags &= ~READWRITE
    nil
  end

  def size
    if @string.nil?
      raise IOError, "not opened"
    end
    @string.length
  end
  alias length size

  def seek(amount, whence=IO::SEEK_SET)
    raise IOError, "closed stream" unless !closed?
    offset = amount
    case whence
    when 0
    when 1
      offset += @pos
    when 2
      offset += @string.length
    else
      raise Errno::EINVAL, "invalid whence"
    end
    if offset < 0
      raise Errno::EINVAL
    end
    @pos = offset
    0
  end

  def print(*strings)
    strings.each do |string|
      str = string.to_s
      write str
    end
    nil
  end

  def puts(*strings)
    strings.each do |string|
      str = string.to_s
      write str
      if str.length == 0 || str[str.length-1] != DEFAULT_RS
        write DEFAULT_RS
      end
    end
    nil
  end

  def sysread(*args)
    str = read(*args)
    if str == nil
      raise EOFError, "end of file reached"
    end
    str
  end

  def getc
    raise IOError, "not opened for reading" unless readable?

    if @string.length <= @pos
      return nil
    end

    c = @string[@pos]
    @pos += 1
    c
  end

  def each(*args, &block)
    return to_enum :each unless block
    while line = gets(*args)
      block.call(line)
    end
  end

  def fileno
    nil
  end

  private

  def readable?
    (@flags & READABLE) == READABLE
  end

  def eof?
    @pos >= @string.length
  end
  alias_method :eof, :eof?

  def tty?
    false
  end
  alias_method :isatty, :tty?

  def sync
    true
  end

  def sync=(val)
    val
  end
end
