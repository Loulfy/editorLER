//
// Created by loulfy on 24/02/2023.
//

#include "ler_sys.hpp"
#include "ler_log.hpp"

#include <miniz.h>
#include <miniz_zip.h>

#ifdef _WIN32
    #include <Windows.h>
    #include <ShlObj.h>
#else
    #include <unistd.h>
    #include <pwd.h>
#endif

namespace ler
{
    std::string getHomeDir()
    {
        #ifdef _WIN32
        wchar_t wide_path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, SHGFP_TYPE_CURRENT, wide_path))) {
            char path[MAX_PATH];
            if (WideCharToMultiByte(CP_UTF8, 0, wide_path, -1, path, MAX_PATH, nullptr, nullptr) > 0) {
                return path;
            }
        }
        #else
        struct passwd* pw = getpwuid(getuid());
        if (pw != nullptr)
            return pw->pw_dir;
        #endif

        return {};
    }

    StdFileSystem::StdFileSystem(const fs::path& root) : m_root(root.lexically_normal())
    {

    }

    bool StdFileSystem::exists(const fs::path& path) const
    {
        return fs::exists(m_root / path);
    }

    Blob StdFileSystem::readFile(const fs::path& path)
    {
        // Open the stream to 'lock' the file.
        const fs::path name = m_root / path;
        std::ifstream f(name, std::ios::in | std::ios::binary);

        // Obtain the size of the file.
        std::error_code ec;
        const auto sz = static_cast<std::streamsize>(fs::file_size(name, ec));
        if(ec.value())
            throw std::runtime_error("File Not Found: " + ec.message());

        // Create a buffer.
        Blob result(sz);
        //auto blob = std::make_shared<Blobi>();
        //blob->data.resize(sz);

        // Read the whole file into the buffer.
        f.read(result.data(), sz);
        //f.read(blob->data.data(), sz);

        return result;
    }

    void StdFileSystem::enumerates(std::vector<fs::path>& entries)
    {
        for(const auto& entry : fs::recursive_directory_iterator(m_root))
        {
            std::string spath = entry.path().lexically_normal().generic_string();
            std::string relative = spath.substr(m_root.string().size() + 1);
            if(entry.is_regular_file())
                entries.emplace_back(relative);
        }
    }

    fs::file_time_type StdFileSystem::last_write_time(const fs::path& path)
    {
        return fs::last_write_time(path);
    }

    /*std::future<Blob> StdFileSystem::readFileAsync(const fs::path& path)
    {
        return Async::GetPool().submit([](){
            Blob res;
            return res;
        });
    }*/

    ZipFileSystem::~ZipFileSystem()
    {
        if (m_zipArchive)
        {
            mz_zip_reader_end((mz_zip_archive*)m_zipArchive);
            free(m_zipArchive);
            m_zipArchive = nullptr;
        }
    }

    ZipFileSystem::ZipFileSystem(const fs::path& path)
    {
        auto m_ArchivePath = path.lexically_normal().generic_string();

        m_zipArchive = malloc(sizeof(mz_zip_archive));
        memset(m_zipArchive, 0, sizeof(mz_zip_archive));

        if (!mz_zip_reader_init_file((mz_zip_archive*)m_zipArchive, m_ArchivePath.c_str(),
            MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY | MZ_ZIP_FLAG_VALIDATE_HEADERS_ONLY))
        {
            const char* errorString = mz_zip_get_error_string(mz_zip_get_last_error((mz_zip_archive*)m_zipArchive));
            log::warn(errorString);
        }

        mz_uint numFiles = mz_zip_reader_get_num_files((mz_zip_archive*)m_zipArchive);
        for (mz_uint i = 0; i < numFiles; i++)
        {
            mz_uint nameLength = mz_zip_reader_get_filename((mz_zip_archive*)m_zipArchive, i, nullptr, 0);
            std::string name;
            name.resize(nameLength - 1); // exclude the trailing zero
            mz_zip_reader_get_filename((mz_zip_archive*)m_zipArchive, i, name.data(), nameLength);

            if (!mz_zip_reader_is_file_a_directory((mz_zip_archive*)m_zipArchive, i))
                m_files[name] = i;
        }
    }

    bool ZipFileSystem::exists(const fs::path& path) const
    {
        return m_files.contains(path);
    }

    Blob ZipFileSystem::readFile(const fs::path& path)
    {
        std::string normalizedName = path.lexically_normal().relative_path().generic_string();

        if (normalizedName.empty())
            return {};

        auto entry = m_files.find(normalizedName);
        if (entry == m_files.end())
            return {};

        uint32_t fileIndex = entry->second;

        // working with the archive from now on, requires synchronous access
        std::lock_guard<std::mutex> lockGuard(m_mutex);

        // get information about the file, including its uncompressed size
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat((mz_zip_archive*)m_zipArchive, fileIndex , &stat))
        {
            const char* errorString = mz_zip_get_error_string(mz_zip_get_last_error((mz_zip_archive*)m_zipArchive));
            log::warn(errorString);
            return {};
        }

        if (stat.m_uncomp_size == 0)
            return {};

        // extract the file
        Blob uncompressedData(stat.m_uncomp_size);
        if (!mz_zip_reader_extract_to_mem((mz_zip_archive*)m_zipArchive, fileIndex, uncompressedData.data(), stat.m_uncomp_size, 0))
        {
            const char* errorString = mz_zip_get_error_string(mz_zip_get_last_error((mz_zip_archive*)m_zipArchive));
            log::warn(errorString);
            return {};
        }

        return uncompressedData;
    }

    void ZipFileSystem::enumerates(std::vector<fs::path>& entries)
    {
        for(const auto& entry : m_files)
            entries.emplace_back(entry.first);
    }

    fs::file_time_type ZipFileSystem::last_write_time(const fs::path& path)
    {
        std::string normalizedName = path.lexically_normal().relative_path().generic_string();

        if (normalizedName.empty())
            return {};

        auto entry = m_files.find(normalizedName);
        if (entry == m_files.end())
            return {};

        uint32_t fileIndex = entry->second;

        // working with the archive from now on, requires synchronous access
        std::lock_guard<std::mutex> lockGuard(m_mutex);

        // get information about the file, including its uncompressed size
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat((mz_zip_archive*)m_zipArchive, fileIndex , &stat))
        {
            const char* errorString = mz_zip_get_error_string(mz_zip_get_last_error((mz_zip_archive*)m_zipArchive));
            log::warn(errorString);
            return {};
        }

        return std::chrono::clock_cast<std::chrono::file_clock>(std::chrono::system_clock::from_time_t(stat.m_time));
    }

    //std::future<Blob> ZipFileSystem::readFileAsync(const fs::path &path) {return {}; }

    void FileSystemService::mount(const FileSystemPtr& fs)
    {
        m_mountPoints.emplace_back(fs);
    }

    FileSystemService& FileSystemService::Get()
    {
        static FileSystemService service;
        return std::ref(service);
    }

    bool FileSystemService::exists(const fs::path& path) const
    {
        return std::ranges::any_of(m_mountPoints, [path](auto& fs){ return fs->exists(path); });
    }

    Blob FileSystemService::readFile(const fs::path& path)
    {
        for(auto& fs : m_mountPoints)
        {
            if(fs->exists(path))
                return fs->readFile(path);
        }
        return {};
    }

    void FileSystemService::enumerates(std::vector<fs::path>& entries)
    {
        for(auto& fs : m_mountPoints)
            fs->enumerates(entries);
    }

    fs::file_time_type FileSystemService::last_write_time(const fs::path& path)
    {
        for(auto& fs : m_mountPoints)
        {
            if(fs->exists(path))
                return fs->last_write_time(path);
        }
        return {};
    }
}
