#ifndef CORE_PLATFORM_PLATFORM_HPP
#define CORE_PLATFORM_PLATFORM_HPP

class Platform {
 public:
  Platform();
  ~Platform();

  Platform(const Platform&) = delete;
  Platform& operator=(const Platform&) = delete;
  Platform(Platform&&) = delete;
  Platform& operator=(Platform&&) = delete;

 private:
  bool initialized_{};
};

#endif
