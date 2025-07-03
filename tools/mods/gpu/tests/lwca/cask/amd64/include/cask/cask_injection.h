#pragma once

namespace cask{

// Singleton class that manages injection libraries
struct InjectManager {
private:
    InjectManager();
    ~InjectManager();
public:
    InjectManager(InjectManager const&) = delete;
    void operator=(InjectManager const&) = delete;
    static InjectManager& instance();
    void loadInjections() const;
};

} // namespace cask