//
// Created by loulfy on 09/03/2023.
//

#ifndef LER_SYS_H
#define LER_SYS_H

#include "common.hpp"
#include <concepts>
#include <coroutine>
#include <exception>
#include <BS_thread_pool.hpp>

namespace ler
{
    static const fs::path ASSETS_DIR = fs::path(PROJECT_DIR) / "assets";
    static const fs::path CACHED_DIR = fs::path("cached");
    static constexpr uint32_t C50Mio = 50 * 1024 * 1024;
    std::string getHomeDir();

    class Async
    {
    public:

        static BS::thread_pool& GetPool()
        {
            static Async async;
            return std::ref(async.m_pool);
        }

    private:

        BS::thread_pool m_pool;
    };

    enum class BlobKind
    {
        Image,
        Scene,
        Shader
    };

    struct Blobi
    {
        using allocator_type = std::pmr::polymorphic_allocator<char>;
        explicit Blobi(allocator_type alloc = {}) : path(alloc), data(alloc) {}
        Blobi(const Blobi& blob, const allocator_type& alloc) : path(blob.path, alloc), data(blob.data, alloc) {}
        Blobi(Blobi&& blob, const allocator_type& alloc) : path(std::move(blob.path), alloc), data(std::move(blob.data), alloc) {}

        std::pmr::string path;
        std::pmr::vector<char> data;
        fs::file_time_type time;
        BlobKind kind = BlobKind::Shader;
    };

    using Blob = std::vector<char>;
    //using Blob = std::shared_ptr<Blobi>;

    template <typename T>
    class AsyncRes
    {
    public:
        struct promise_type
        {
            T result;
            std::future<Blob> awaiter;
            AsyncRes get_return_object() { return AsyncRes(handle_type::from_promise(*this)); }
            std::suspend_never initial_suspend() { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }
            void return_value(T res) noexcept
            {
                result = std::move(res);
            }
            void unhandled_exception() {}
        };
        using handle_type = std::coroutine_handle<promise_type>;
        explicit AsyncRes(handle_type h) : handle(h) {}
        [[nodiscard]] bool loaded() const { return handle.done(); }
        void wait() { handle.promise().awaiter.wait(); }
        ~AsyncRes() { handle.destroy(); }

    private:
        handle_type handle;
    };

    class IFileSystem
    {
    public:

        virtual ~IFileSystem() = default;
        virtual Blob readFile(const fs::path& path) = 0;
        [[nodiscard]] virtual bool exists(const fs::path& path) const = 0;
        virtual void enumerates(std::vector<fs::path>& entries) = 0;
        [[nodiscard]] virtual fs::file_time_type last_write_time(const fs::path& path) = 0;
        //virtual std::future<Blob> readFileAsync(const fs::path& path) = 0;
        //virtual TaskBlob readFileAwaitable(const fs::path& path) = 0;
    };

    using FileSystemPtr = std::shared_ptr<IFileSystem>;

    template <typename Derived>
    class FileSystem : public IFileSystem
    {
    public:

        static FileSystemPtr Create(const fs::path& path) { return std::make_shared<Derived>(path); }
    };

    class StdFileSystem : public FileSystem<StdFileSystem>
    {
    public:

        explicit StdFileSystem(const fs::path& root);
        Blob readFile(const fs::path& path) override;
        [[nodiscard]] bool exists(const fs::path& path) const override;
        void enumerates(std::vector<fs::path>& entries) override;
        [[nodiscard]] fs::file_time_type last_write_time(const fs::path& path) override;
        //std::future<Blob> readFileAsync(const fs::path& path) override;
        //static FileSystemPtr Create(const fs::path& path) { return std::make_shared<StdFileSystem>(path); }

    private:

        fs::path m_root;
    };

    class ZipFileSystem : public FileSystem<ZipFileSystem>
    {
    public:

        ~ZipFileSystem() override;
        explicit ZipFileSystem(const fs::path& path);
        Blob readFile(const fs::path& path) override;
        [[nodiscard]] bool exists(const fs::path& path) const override;
        void enumerates(std::vector<fs::path>& entries) override;
        [[nodiscard]] fs::file_time_type last_write_time(const fs::path& path) override;
        //std::future<Blob> readFileAsync(const fs::path& path) override;
        //static FileSystemPtr Create(const fs::path& path) { return std::make_shared<ZipFileSystem>(path); }

    private:

        std::mutex m_mutex;
        void* m_zipArchive = nullptr;
        std::unordered_map<fs::path, uint32_t> m_files;
    };

    class FileSystemService : public IFileSystem
    {
    public:

        void mount(const FileSystemPtr& fs);
        static FileSystemService& Get();

        Blob readFile(const fs::path& path) override;
        [[nodiscard]] bool exists(const fs::path& path) const override;
        void enumerates(std::vector<fs::path>& entries) override;
        [[nodiscard]] fs::file_time_type last_write_time(const fs::path& path) override;
        //std::future<Blob> readFileAsync(const fs::path& path) override;

    private:

        std::vector<FileSystemPtr> m_mountPoints;
        std::byte Buffer[C50Mio];
        std::pmr::monotonic_buffer_resource m_pool;
    };

    class ReadFileAwaitable
    {
    public:

        auto operator co_await()
        {
            struct Awaiter
            {
                std::coroutine_handle<>* hp_;
                constexpr bool await_ready() const noexcept { return false; }
                void await_suspend(std::coroutine_handle<> h)
                {

                }
                constexpr void await_resume() const noexcept {}
            };
            return Awaiter();
        }
    };
}

#endif //LER_SYS_H
