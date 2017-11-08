#include <iostream>
#include <iomanip>
#include <fstream>
#include <stdexcept>
#include <string>
#include <cstdlib>
#include <boost/variant.hpp>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/utility/string_view.hpp>

namespace project {
    using boost::optional;
    namespace fs = boost::filesystem;
    namespace logging = boost::log;
    using boost::iostreams::mapped_file;
    using string_view = std::string_view;
    template<class...Ts> using variant = boost::variant<Ts...>;

    template<class...Args>
    inline decltype(auto) visit(Args&& ...args) { return boost::apply_visitor(std::forward<Args>(args)...); }

    struct empty_type
    {
        constexpr bool operator ==(empty_type const&) const { return true; }

        friend std::ostream& operator <<(std::ostream& os, empty_type const&)
        {
            return os << "{ empty }";
        }

        friend std::ostream& write(std::ostream& os, empty_type const& e, std::size_t indent)
        {
            return os << std::string(indent, ' ') << e;
        }
    };

    constexpr auto empty = empty_type();
}

#define PROJECT_LOG(x) BOOST_LOG_TRIVIAL(x)

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


template<class Outer>
struct ImplementIndentWrite
{
    friend std::ostream& write(std::ostream& os, ImplementIndentWrite const& wf, std::size_t indent = 0)
    {
        return static_cast<Outer const&>(wf).write_impl(os, indent);
    }

    friend std::ostream& operator <<(std::ostream& os, ImplementIndentWrite const& wf)
    {
        return write(os, static_cast<Outer const&>(wf));
    }

};

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

    for (; first != last; ++first, shift += 8) {
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
    using endian = little_endian; static constexpr std::size_t size = 2;
};
struct uint32_desc
{
    using endian = little_endian; static constexpr std::size_t size = 4;
};
struct id_desc
{
    using endian = big_endian; static constexpr std::size_t size = 4;
};
template<class Descriptor> constexpr std::size_t next_offset = Descriptor::offset + Descriptor::size;

struct WaveFmt
    : ImplementIndentWrite<WaveFmt>
{
    using string_view = project::string_view;

    struct chunk_id_desc
        : id_desc
    {
        static constexpr std::size_t offset = 0;
    };
    struct chunk_size_desc
        : uint32_desc
    {
        static constexpr std::size_t offset = next_offset<chunk_id_desc>;
    };
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
        : buffer_(buffer)
        , size_(size)
    {
        if (chunk_id() != "fmt ") throw std::invalid_argument("WaveFmt - invalid chunk id");
    }

    string_view chunk_id() const
    {
        return string_view(reinterpret_cast<char const *>(buffer_ + chunk_id_desc::offset),
                           chunk_id_desc::size);
    }

    std::size_t chunk_size() const { return extract_integer<chunk_size_desc>(buffer_); }
    std::size_t extent() const { return chunk_size() + next_offset<chunk_size_desc>; }

    auto audio_format() const { return extract_integer<audio_format_desc>(buffer_); }

    auto num_channels() const { return extract_integer<num_channels_desc>(buffer_); }
    auto sample_rate() const { return extract_integer<sample_rate_desc>(buffer_); }
    auto byte_rate() const { return extract_integer<byte_rate_desc>(buffer_); }
    auto block_align() const { return extract_integer<block_align_desc>(buffer_); }
    auto bits_per_sample() const { return extract_integer<bits_per_sample_desc>(buffer_); }

    std::ostream& write_impl(std::ostream& os, std::size_t indent) const
    {
        constexpr auto lf     = '\n';
        auto           prefix = std::string(indent, os.fill());
        os.width(0);
        os << prefix << "TYPE     : " << chunk_id() << lf;
        os << prefix << "SIZE     : " << chunk_size() << lf;
        os << prefix << "FORMAT   : " << audio_format() << lf;
        os << prefix << "CHANNELS : " << num_channels() << lf;
        os << prefix << "RATE     : " << sample_rate() << lf;
        os << prefix << "BYTE RATE: " << byte_rate() << lf;
        os << prefix << "ALIGN    : " << block_align() << lf;
        os << prefix << "BITS PER : " << bits_per_sample();
        return os;
    }

private:
    std::uint8_t const *buffer_;
    std::size_t        size_;
};

struct Unknown
    : ImplementIndentWrite<Unknown>
{
    using string_view = project::string_view;

    struct chunk_id_desc
        : id_desc
    {
        static constexpr std::size_t offset = 0;
    };

    Unknown(std::uint8_t const *buffer, std::size_t size)
        : buffer_(buffer)
        , size_(size)
    {
    }

    string_view chunk_id() const
    {
        return string_view(reinterpret_cast<char const *>(buffer_ + chunk_id_desc::offset),
                           chunk_id_desc::size);
    }

    std::size_t chunk_size() const { return size_ - chunk_id_desc::size; }
    std::size_t extent() const { return size_; }

    std::ostream& write_impl(std::ostream& os, std::size_t indent) const
    {
        constexpr auto lf     = '\n';
        auto           prefix = std::string(indent, os.fill());
        os.width(0);
        os << prefix << "TYPE     : " << chunk_id() << lf;
        return os;
    }

private:
    std::uint8_t const *buffer_;
    std::size_t        size_;

};

using WaveChunk = project::variant<Unknown, WaveFmt>;

struct Wave
    : ImplementIndentWrite<Wave>
{
    using string_view = project::string_view;

    struct chunk_id_desc
        : id_desc
    {
        static constexpr std::size_t offset = 0;
    };
    struct first_chunk_desc
        : id_desc
    {
        static constexpr std::size_t offset = next_offset<chunk_id_desc>;
    };

    Wave(std::uint8_t const *buffer, std::size_t size)
        : buffer_(buffer)
        , size_(size)
    {
        auto offset = first_chunk_desc::offset;
        while (offset < size) {
            chunks_.emplace_back(deduce_chunk(offset, size - offset));
            offset += project::visit([](auto&& chunk) { return chunk.extent(); }, chunks_.back());
        }
    }

    string_view chunk_id() const
    {
        return string_view(reinterpret_cast<char const *>(buffer_ + chunk_id_desc::offset), chunk_id_desc::size);
    }

    string_view chunk_id_at(std::size_t offset) const
    {
        return string_view(reinterpret_cast<char const *>(buffer_ + offset), first_chunk_desc::size);
    }

    WaveChunk deduce_chunk(std::size_t offset, std::size_t remaining) const
    {
        auto id = chunk_id_at(offset);
        if (id == "fmt ") {
            return WaveFmt(buffer_ + offset, remaining);
        }
        else {
            return Unknown(buffer_ + offset, remaining);
        }
    }

    std::ostream& write_impl(std::ostream& os, std::size_t indent = 0) const
    {
        constexpr auto nl     = '\n';
        auto           prefix = std::string(indent, os.fill());
        os.width(0);
        os << prefix << "TYPE  : " << chunk_id() << '\n';
        const char* biff = "";
        for (std::size_t i = 0; i < chunks_.size(); ++i) {
            os << biff << prefix << "CHUNK " << i << nl;
            project::visit([indent, &os](auto&& chunk)
                           {
                               write(os, chunk, indent + 4);
                           }, chunks_[i]);
            biff = "\n";
        }
        return os;
    }


private:
    std::uint8_t const     *buffer_;
    std::size_t            size_;
    std::vector<WaveChunk> chunks_;
};

using RiffChunk = project::variant<project::empty_type, Wave>;

struct Riff
{
    using string_view = project::string_view;
    enum
        : std::size_t
    {
        chunk_id_offset   = 0,
        chunk_size_offset = 4,
        next_chunk_offset = 8,
        minimum_size      = next_chunk_offset + 4
    };

    Riff(std::uint8_t const *buffer, std::size_t size)
        : buffer_(buffer)
        , size_(size)
    {
        if (size < minimum_size) throw std::invalid_argument("Riff - too small: " + std::to_string(size));
        if (chunk_id() != "RIFF") throw std::invalid_argument("Riff - invalid chunk id: "s + std::string(chunk_id()));
        if (next_chunk_id() == "WAVE") next_chunk_ = Wave(buffer_ + next_chunk_offset, size_ - next_chunk_offset);
    }

    string_view chunk_id() const { return string_view(reinterpret_cast<char const *>(buffer_), 4); }

    string_view next_chunk_id() const
    {
        return string_view(reinterpret_cast<char const *>(buffer_ + next_chunk_offset), 4);
    }

    std::size_t chunk_size() const
    {
        std::size_t result = 0;
        int         shift  = 0;
        auto        first  = buffer_ + chunk_size_offset;
        auto        last   = first + 4;

        for (; first != last; ++first, shift += 8) {
            result |= (std::size_t(*first) << shift);
        }

        return result;
    }

    std::ostream& write_impl(std::ostream& os, std::size_t indent = 0) const
    {
        os << "TYPE  : " << chunk_id() << '\n';
        os << "LENGTH: " << chunk_size() << '\n';
        os << "NEXT  : " << next_chunk_id() << '\n';
        os << "detail:\n";
        project::visit([&](auto&& x) -> std::ostream& { return write(os, x, 4); }, next_chunk_);
        return os;
    }

    friend std::ostream& operator <<(std::ostream& os, Riff const& riff)
    {
        return riff.write_impl(os);
    }

private:
    std::uint8_t const *buffer_;
    std::size_t        size_;
    RiffChunk          next_chunk_;
};

int main()
{
    init_logging();
    auto                 path = project::fs::path(safe_env("HOME").value_or_eval([] { return "/"; }))
                                / file_path;

    PROJECT_LOG(debug) << "opening: " << path << std::endl;
    project::mapped_file riff_file(path.string(), project::mapped_file::readonly);
    auto                 riff = Riff(reinterpret_cast<std::uint8_t const *>(riff_file.const_data()), riff_file.size());
    std::cout << riff << std::endl;


    return 0;
}