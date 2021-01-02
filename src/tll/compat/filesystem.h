#ifndef _TLL_COMPAT_FILESYSTEM_H
#define _TLL_COMPAT_FILESYSTEM_H

#if __has_include(<filesystem>)
# include <filesystem>
#else
# include <experimental/filesystem>
namespace std::filesystem { using namespace ::std::experimental::filesystem; }
#endif

#endif//_TLL_COMPAT_FILESYSTEM_H
