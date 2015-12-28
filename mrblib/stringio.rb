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

  def gets(*args)
    raise IOError, "not opened for reading" unless readable?

    str = nil
    limit = 0

    case args.length
    when 0
      str = $/ || "\n"
    when 1
      str = args[0]
      if !str.nil? && !str.kind_of?(String)
        if str.respond_to?(:to_str)
          str = str.to_str
        else
          limit = str
          return "" if limit == 0
          str = $/ || "\n"
        end
      end
    when 2
      str = args[0]
      lim = args[1]
      if !str.nil? && !str.kind_of?(String)
        raise TypeError, "no implicit conversion of #{str.class} into String"
      end
      if !lim.nil? && !lim.kind_of?(Integer)
        raise TypeError, "no implicit conversion of #{str.class} into Integer"
      end
      limit = if lim.nil?
        0
      else
        limit = lim.to_i
      end
    else
      raise ArgumentError, "wrong number of arguments (#{args.length} for 0..2)"
    end

    return nil if (@string.length <= @pos)

    e = @string.length

    if 0 < limit && limit < e
      str = @string[@pos, limit]
    end

    str = if str.nil?
      @string[@pos, @string.length]
    elsif str.length == 0
      index = 0
      while @string[index] == "\n"
        index += 1
        return nil if index == @string.length
      end
      save_index = index
      while (p = @string[index, @string.length - index].index("\n")) && index != @string.length
        p += 1
        if @string[p] == "\n"
          e = p + 1
          break
        end
      end
      @string[save_index, e - save_index]
    elsif str.length == 1
      if p = @string.index(str, @pos)
        e = p + 1
      end
      @string[@pos, e - @pos]
    else
      if str.length < @string.length
        if p = @string.index(str, @pos)
          e = p + str.length
        end
      end
      @string[@pos, e - @pos]
    end

    @pos = e
    @lineno += 1
    str
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

  def writable?
    (@flags & WRITABLE) == WRITABLE
  end

  def readable?
    (@flags & READABLE) == READABLE
  end

  def append?
    (@flags & APPEND) == APPEND
  end

  def eof?
    @pos >= @string.length
  end
  alias_method :eof, :eof?
end
