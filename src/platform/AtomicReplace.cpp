#include "platform/AtomicReplace.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <random>
#include <stdexcept>
#include <system_error>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace openemperor::platform {
namespace {
namespace fs=std::filesystem;
int create_unique(const fs::path& path) {
#if defined(_WIN32)
    return ::_wopen(path.c_str(),_O_WRONLY|_O_CREAT|_O_EXCL|_O_BINARY|_O_NOINHERIT,
                    _S_IREAD|_S_IWRITE);
#else
    return ::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
#endif
}
int write_bytes(int fd,const char* data,std::size_t count) {
#if defined(_WIN32)
    return ::_write(fd,data,static_cast<unsigned>(std::min<std::size_t>(count,INT_MAX)));
#else
    return static_cast<int>(::write(fd,data,count));
#endif
}
int flush_bytes(int fd) {
#if defined(_WIN32)
    return ::_commit(fd);
#else
    return ::fsync(fd);
#endif
}
int close_file(int fd) {
#if defined(_WIN32)
    return ::_close(fd);
#else
    return ::close(fd);
#endif
}
} // namespace

void atomic_replace(const fs::path& target,std::string_view bytes,
                    const std::function<void()>& before_rename,AtomicWriteFault fault) {
    std::random_device random;
    fs::path temporary;
    int fd=-1;
    for (int attempt=0;attempt<16 && fd<0;++attempt) {
        temporary=target.parent_path()/(target.filename().string()+".tmp-"+
            std::to_string(random())+"-"+std::to_string(random()));
        fd=create_unique(temporary);
        if (fd<0 && errno!=EEXIST) throw std::system_error(errno,std::generic_category(),"create save temp");
    }
    if (fd<0) throw std::runtime_error("cannot allocate unique save temp file");
    bool committed=false;
    try {
        if (fault==AtomicWriteFault::BeforeWrite) throw std::runtime_error("injected save write failure");
        std::size_t written=0;
        while (written<bytes.size()) {
            const auto count=write_bytes(fd,bytes.data()+written,
                fault==AtomicWriteFault::AfterPartialWrite ? 1 : bytes.size()-written);
            if (count<0 && errno==EINTR) continue;
            if (count<=0) throw std::system_error(errno,std::generic_category(),"write save temp");
            written+=static_cast<std::size_t>(count);
            if (fault==AtomicWriteFault::AfterPartialWrite)
                throw std::runtime_error("injected partial save write failure");
        }
        if (flush_bytes(fd)!=0) throw std::system_error(errno,std::generic_category(),"flush save temp");
        if (close_file(fd)!=0) { fd=-1; throw std::system_error(errno,std::generic_category(),"close save temp"); }
        fd=-1;
        if (fault==AtomicWriteFault::BeforeRename) throw std::runtime_error("injected save rename failure");
        before_rename();
        fs::rename(temporary,target);
        committed=true;
    } catch (...) {
        if (fd>=0) close_file(fd);
        if (!committed) { std::error_code ignored; fs::remove(temporary,ignored); }
        throw;
    }
}
} // namespace openemperor::platform
