class IOError < StandardError
end

class StringIO
  READABLE  = 0x0001
  WRITABLE  = 0x0002
  READWRITE = READABLE | WRITABLE
  BINMODE   = 0x0004
  APPEND    = 0x0040
  CREATE    = 0x0080
  TRUNC     = 0x0800

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

  def initialize(string = "", mode = "r+")
    flags = 0
    if mode.kind_of?(String)
      flags = modestr_fmode(mode)
      if (flags & TRUNC) == TRUNC
        string.replace ""
      end
    end

    @string = string
    @pos = 0
    @lineno = 0
    @flags = flags
  end

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

  def write(str)
    raise IOError, "not opened for writing" unless writable?
    str = str.to_s

    return 0 if str.length == 0

    if append?
      @pos = @string.length
    end

    @string.replace(@string[0, @pos] + str)
    str.length
  end

  def read(length = nil, buf = "")
    raise IOError, "not opened for reading" unless readable?

    length = @string.length unless length
    buf = "" unless buf

    raise ArgumentError unless 0 <= length

    ret = @string[@pos, length]
    buf.replace ret
    @pos += length
    ret
  end

  # call-seq:
  #   strio.gets(sep=$/)     -> string or nil
  #   strio.gets(limit)      -> string or nil
  #   strio.gets(sep, limit) -> string or nil
  def gets(*args)
    raise IOError, "not opened for reading" unless readable?

    str = nil
    limit = nil

    case args.length
    when 0
      str = $/ || "\n"
    when 1
      str = args[0]
      if !str.nil? && str.kind_of?(String)
        if str.respond_to?(:to_str)
          str = str.to_str
        else
          str = $/ || "\n"
          limit = str
          return "" if limit == 0
        end
      end
    when 2
      str = args[0]
      lim = args[1]
      raise TypeError if !str.nil? && str.kind_of?(String)
      raise TypeError if !lim.nil? && lim.kind_of?(Numeric)
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
      if p = @string.index(str)
        e = p + 1
      end
      @string[@pos, e]
    else
      if str.length < @string.length
        if (p = @string.index(str)) == 0
          e = p + str.length
        end
      end
      @string[@pos, e]
    end

    @pos = e
    @lineno += 1
    str
  end

  private

  def modestr_fmode(mode)
    flags = 0
    index = 0

    case mode[index]
    when 'r'
      flags |= READABLE
    when 'w'
      flags |= WRITABLE | CREATE | TRUNC
    when 'a'
      flags |= WRITABLE | APPEND | CREATE
    else
      raise ArgumentError, "illegal access mode #{mode}"
    end
    index += 1

    while mode[index]
      case mode[index]
      when 'b'
        flags |= BINMODE
      when '+'
        flags |= READWRITE
      else
        raise ArgumentError, "illegal access mode #{mode}"
      end
      index += 1
    end

    flags
  end

  def writable?
    (@flags & WRITABLE) == WRITABLE
  end

  def readable?
    (@flags & READABLE) == READABLE
  end

  def append?
    (@flags & APPEND) == APPEND
  end
end
