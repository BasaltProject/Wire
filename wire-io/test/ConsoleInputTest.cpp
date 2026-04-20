#include <basalt/wire/io/ConsoleInput.hpp>

#include <doctest.h>

#include <string>

#if !defined(_WIN32)
#include <csignal>
#include <future>
#include <pthread.h>
#include <thread>
#include <unistd.h>
#endif

using namespace basalt::wire::io;

TEST_CASE("console lines handle CRLF final partial input and overflow without splitting commands") {
    const std::string source = "help\r\n123456789\nstop\rlast";
    std::size_t at = 0;
    ConsoleInput input{4, [](auto) { return true; }, [&] { return at == source.size() ? -1 : source[at++]; }};
    CHECK(input.readLine({}) == "help");
    const auto oversized = input.readLine({});
    REQUIRE_FALSE(oversized);
    CHECK(oversized.error() == ConsoleInputError::kTooLong);
    CHECK(input.readLine({}) == "stop");
    CHECK(input.readLine({}) == "last");
    const auto end = input.readLine({});
    REQUIRE_FALSE(end);
    CHECK(end.error() == ConsoleInputError::kEnd);
}

TEST_CASE("console input stops while awaiting the rest of a partial line") {
    std::stop_source stopping;
    std::size_t bytes = 0;
    ConsoleInput input{4,
                       [&](auto timeout) {
                           CHECK(timeout.count() <= 25);
                           if (bytes == 2) {
                               stopping.request_stop();
                               return false;
                           }
                           return true;
                       },
                       [&] {
                           ++bytes;
                           return 'a';
                       }};
    const auto stopped = input.readLine(stopping.get_token());
    REQUIRE_FALSE(stopped);
    CHECK(stopped.error() == ConsoleInputError::kStopped);
    CHECK(bytes == 2);
}

#if !defined(_WIN32)

TEST_CASE("signal interrupting console readiness cannot enter a blocking read") {
    using namespace std::chrono_literals;

    struct InputPipe {
        int saved = ::dup(STDIN_FILENO);
        int descriptors[2]{-1, -1};

        InputPipe() {
            REQUIRE(saved >= 0);
            REQUIRE(::pipe(descriptors) == 0);
            REQUIRE(::dup2(descriptors[0], STDIN_FILENO) >= 0);
        }

        void closeWriter() {
            if (descriptors[1] >= 0)
                ::close(descriptors[1]);
            descriptors[1] = -1;
        }

        ~InputPipe() {
            ::dup2(saved, STDIN_FILENO);
            ::close(saved);
            ::close(descriptors[0]);
            closeWriter();
        }
    } input;

    const auto previous = std::signal(SIGUSR1, +[](int) {});
    REQUIRE(previous != SIG_ERR);
    std::promise<void> started;
    auto starting = started.get_future();
    std::promise<ConsoleInputError> completed;
    auto completion = completed.get_future();
    std::jthread reader{[&](std::stop_token stop) {
        ConsoleInput console;
        started.set_value();
        const auto result = console.readLine(stop);
        completed.set_value(result ? ConsoleInputError::kEnd : result.error());
    }};
    starting.wait();
    std::this_thread::sleep_for(10ms);
    CHECK(::pthread_kill(reader.native_handle(), SIGUSR1) == 0);
    std::this_thread::sleep_for(10ms);
    reader.request_stop();
    const auto finished = completion.wait_for(250ms);
    CHECK(finished == std::future_status::ready);
    input.closeWriter();
    reader.join();
    std::signal(SIGUSR1, previous);
    CHECK(completion.get() == ConsoleInputError::kStopped);
}
#endif
