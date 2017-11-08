#include "project.hpp"
#include "implement_indent_write.hpp"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <stdexcept>
#include <string>
#include <cstdlib>

using namespace std::literals;

project::optional<std::string> safe_env(const char *name)
{
    project::optional<std::string> result;
    auto                           pvar = std::getenv(name);
    if (pvar) result.emplace(pvar);
    return result;
}

namespace fs = boost::filesystem;

namespace {
    const auto file_path = fs::path("Documents/525_seek_2ms.wav");
}

void init_logging()
{
    using namespace project;
    logging::core::get()->set_filter
                            (
                                logging::trivial::severity >= logging::trivial::debug
                            );
}


unsigned int upscale(char c)
{
    return static_cast<unsigned int>(c);
}

struct little_endian
{
};
struct big_endian
{
};

template<std::size_t Size>
struct smallest_integer_type;
template<>
struct smallest_integer_type<2>
{
    using type = std::uint16_t;
};
template<>
struct smallest_integer_type<4>
{
    using type = std::uint32_t;
};
template<>
struct smallest_integer_type<8>
{
    using type = std::uint64_t;
};
template<std::size_t Size> using smallest_integer_type_t = typename smallest_integer_type<Size>::type;

template<class Type>
Type extract_integer(std::uint8_t const *first, little_endian, std::size_t size = sizeof(Type))
{
    Type result = 0;
    int  shift  = 0;
    auto last   = first + size;

    for (; first != last; ++first, shift += 8)
    {
        result |= (std::size_t(*first) << shift);
    }

    return result;
}

template<class Descriptor>
auto extract_integer(std::uint8_t const *buffer)
{
    using endian = typename Descriptor::endian;
    constexpr auto offset = Descriptor::offset;
    constexpr auto size   = Descriptor::size;
    using type = smallest_integer_type_t<size>;

    return extract_integer<type>(buffer + offset, endian(), size);
}

struct uint16_desc
{
    using endian = little_endian;
    static constexpr std::size_t size = 2;
};
struct uint32_desc
{
    using endian = little_endian;
    static constexpr std::size_t size = 4;
};
struct id_desc
{
    using endian = big_endian;
    static constexpr std::size_t size = 4;
};
template<class Descriptor> constexpr std::size_t next_offset = Descriptor::offset + Descriptor::size;

struct RiffChunk
{
    using string_view = project::string_view;

    struct chunk_id_desc
        : id_desc
    {
        static constexpr std::size_t offset = 0;
    };

    RiffChunk(std::uint8_t const *buffer, std::size_t size)
        : buffer_(buffer)
        , size_(size)
    {
        if (size < chunk_id_desc::size) throw std::invalid_argument("RiffChunk: not enough data for type field");
    }

    string_view chunk_id() const
    {
        return string_view(reinterpret_cast<char const *>(buffer_ + chunk_id_desc::offset), chunk_id_desc::size);
    }

    std::size_t extent() const { return size_; }

    static std::string make_indent(std::ostream& os, std::size_t indent)
    {
        return std::string(indent, os.fill());
    }

    std::ostream& write_impl(std::ostream& os, std::string const& prefix) const
    {
        constexpr auto nl = '\n';
        os.width(0);
        os << prefix << "TYPE  : " << chunk_id();
        return os;
    }

    std::uint8_t const *begin() const { return buffer_; }

    std::uint8_t const *end() const { return buffer_ + size_; }

private:
    std::uint8_t const *buffer_;
    std::size_t        size_;
};

struct SizedRiffChunk
    : RiffChunk
{
    struct chunk_size_desc
        : uint32_desc
    {
        static constexpr std::size_t offset = next_offset<chunk_id_desc>;
    };

    SizedRiffChunk(std::uint8_t const *buffer, std::size_t size)
        : RiffChunk(buffer, size)
    {
        if (size < next_offset<chunk_size_desc>)
            throw std::invalid_argument("SizedRiffChunk - not enough space for size field");
    }

    std::size_t chunk_size() const { return extract_integer<chunk_size_desc>(begin()); }

    std::ostream& write_impl(std::ostream& os, std::string const& prefix) const
    {
        constexpr auto lf = '\n';
        RiffChunk::write_impl(os, prefix);
        os << lf;
        os << prefix << "SIZE  : " << chunk_size();
        return os;
    }
};

struct WaveFmt
    : SizedRiffChunk,
      ImplementIndentWrite<WaveFmt>
{
    struct audio_format_desc
        : uint16_desc
    {
        static constexpr std::size_t offset = next_offset<chunk_size_desc>;
    };
    struct num_channels_desc
        : uint16_desc
    {
        static constexpr std::size_t offset = next_offset<audio_format_desc>;
    };
    struct sample_rate_desc
        : uint32_desc
    {
        static constexpr std::size_t offset = next_offset<num_channels_desc>;
    };
    struct byte_rate_desc
        : uint32_desc
    {
        static constexpr std::size_t offset = next_offset<sample_rate_desc>;
    };
    struct block_align_desc
        : uint16_desc
    {
        static constexpr std::size_t offset = next_offset<byte_rate_desc>;
    };

    struct bits_per_sample_desc
        : uint16_desc
    {
        static constexpr std::size_t offset = next_offset<block_align_desc>;
    };


    WaveFmt(std::uint8_t const *buffer, std::size_t size)
        : SizedRiffChunk(buffer, size)
    {
        if (chunk_id() != "fmt ") throw std::invalid_argument("WaveFmt - invalid chunk id");
    }

    auto audio_format() const { return extract_integer<audio_format_desc>(begin()); }

    auto num_channels() const { return extract_integer<num_channels_desc>(begin()); }

    auto sample_rate() const { return extract_integer<sample_rate_desc>(begin()); }

    auto byte_rate() const { return extract_integer<byte_rate_desc>(begin()); }

    auto block_align() const { return extract_integer<block_align_desc>(begin()); }

    auto bits_per_sample() const { return extract_integer<bits_per_sample_desc>(begin()); }

    std::ostream& write_impl(std::ostream& os, std::string const& prefix) const
    {
        constexpr auto lf = '\n';
        SizedRiffChunk::write_impl(os, prefix);
        os << lf;
        os << prefix << "FORMAT   : " << audio_format() << lf;
        os << prefix << "CHANNELS : " << num_channels() << lf;
        os << prefix << "RATE     : " << sample_rate() << lf;
        os << prefix << "BYTE RATE: " << byte_rate() << lf;
        os << prefix << "ALIGN    : " << block_align() << lf;
        os << prefix << "BITS PER : " << bits_per_sample();
        return os;
    }

};

struct Unknown
    : RiffChunk,
      ImplementIndentWrite<Unknown>
{
    using string_view = project::string_view;

    Unknown(std::uint8_t const *buffer, std::size_t size)
        : RiffChunk(buffer, size)
    {
    }
};

using WaveChunk = project::variant<Unknown, WaveFmt>;

struct Wave
    : RiffChunk,
      ImplementIndentWrite<Wave>
{
    struct first_chunk_desc
        : id_desc
    {
        static constexpr std::size_t offset = next_offset<chunk_id_desc>;
    };

    Wave(std::uint8_t const *buffer, std::size_t size)
        : RiffChunk(buffer, size)
    {
        auto offset = first_chunk_desc::offset;
        while (offset < size)
        {
            chunks_.emplace_back(deduce_chunk(offset, size - offset));
            offset += project::visit([](auto&& chunk) { return chunk.extent(); }, chunks_.back());
        }
    }

    string_view chunk_id_at(std::size_t offset) const
    {
        return string_view(reinterpret_cast<char const *>(begin() + offset), first_chunk_desc::size);
    }

    WaveChunk deduce_chunk(std::size_t offset, std::size_t remaining) const
    {
        auto id = chunk_id_at(offset);
        if (id == "fmt ")
        {
            return WaveFmt(begin() + offset, remaining);
        }
        else
        {
            return Unknown(begin() + offset, remaining);
        }
    }

    std::ostream& write_impl(std::ostream& os, std::string const& prefix) const
    {
        constexpr auto lf = '\n';
        RiffChunk::write_impl(os, prefix);
        os << lf;
        const char       *biff        = "";
        auto             inner_prefix = prefix + "    ";
        for (std::size_t i            = 0; i < chunks_.size(); ++i)
        {
            os << biff << prefix << "CHUNK " << i << lf;
            project::visit([&](auto&& chunk)
                           {
                               chunk.write_impl(os, inner_prefix);
                           }, chunks_[i]);
            biff = "\n";
        }
        return os;
    }

private:
    std::vector<WaveChunk> chunks_;
};

using RiffChunkVariant = project::variant<project::empty_type, Unknown, Wave>;

struct Riff
    : SizedRiffChunk
{
    struct next_chunk_desc
        : id_desc
    {
        static constexpr std::size_t offset = next_offset<chunk_size_desc>;
    };

    static constexpr auto min_possible_size = next_chunk_desc::offset;

    Riff(std::uint8_t const *buffer, std::size_t size)
        : SizedRiffChunk(buffer, size)
    {
        if (size < min_possible_size) throw std::invalid_argument("Riff - too small: " + std::to_string(size));
        if (chunk_id() != "RIFF") throw std::invalid_argument("Riff - invalid chunk id: "s + std::string(chunk_id()));
        auto offset = next_chunk_desc::offset;
        next_chunk_ = deduce_chunk(offset, size - offset);
    }

    RiffChunkVariant deduce_chunk(std::size_t offset, std::size_t remaining) const
    {
        if (offset + chunk_id_desc::size > extent())
            return project::empty;

        auto id = chunk_id_at(offset);
        if (id == "WAVE")
        {
            return Wave(begin() + offset, remaining);
        }
        else
        {
            return Unknown(begin() + offset, remaining);
        }
    }

    string_view chunk_id_at(std::size_t offset) const
    {
        return string_view(reinterpret_cast<char const *>(begin() + offset), next_chunk_desc::size);
    }

    string_view next_chunk_id() const
    {
        return string_view(reinterpret_cast<char const *>(begin() + next_chunk_desc::offset), 4);
    }

    std::ostream& write_impl(std::ostream& os, std::string const& prefix) const
    {
        constexpr auto lf = '\n';
        SizedRiffChunk::write_impl(os, prefix);
        os << lf;
        os << "NEXT  : " << next_chunk_id() << lf;
        os << "CHUNK:\n";
        return project::visit([&](auto&& x) -> std::ostream& { return write(os, x, 4); }, next_chunk_);
    }

    std::ostream& write_impl(std::ostream& os, std::size_t indent = 0) const
    {
        return write_impl(os, make_indent(os, indent));
    }

    friend std::ostream& operator <<(std::ostream& os, Riff const& riff)
    {
        return riff.write_impl(os);
    }

private:
    RiffChunkVariant next_chunk_;
};

int main(int argc, char **argv)
{
    init_logging();

    auto path = argc > 1
                ? project::fs::path(argv[1])
                : project::fs::path(safe_env("HOME").value_or_eval([] { return "/"; }))
                  / file_path;


    PROJECT_LOG(debug) << "opening: " << path << std::endl;

    auto riff_file = project::mapped_file(path.string(), project::mapped_file::readonly);

    auto riff = Riff(reinterpret_cast<std::uint8_t const *>(riff_file.const_data()), riff_file.size());

    std::cout << riff << std::endl;


    return 0;
}