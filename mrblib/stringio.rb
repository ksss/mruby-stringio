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

  attr_accessor :string

  alias_method :tell, :pos

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

  def read_nonblock(*args)
    option = { exception: true }
    if Hash === args.last
      option = args.pop
      if !option.key?(:exception)
        option[:exception] = true
      end
    end

    if args.length == 0
      raise ArgumentError, "wrong number of arguments (given 0, expected 1..2)"
    end

    str = read(*args)
    if str.nil?
      if option[:exception]
        raise EOFError, "end of file reached"
      else
        nil
      end
    end
    str
  end

  def write_nonblock(*args)
    case args.last
    when Hash, NilClass
      args.pop
    end
    write(*args)
  end

  def sysread(*args)
    str = read(*args)
    if str == nil
      raise EOFError, "end of file reached"
    end
    str
  end

  def each(*args, &block)
    return to_enum :each unless block
    while line = gets(*args)
      block.call(line)
    end
  end
  alias each_line each

  def fileno
    nil
  end

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

  def fsync
    0
  end

  def flush
    self
  end
end
