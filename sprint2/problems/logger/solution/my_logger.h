#pragma once

#include <chrono>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <optional>
#include <mutex>
#include <thread>
#include <iostream>

using namespace std::literals;

#define LOG(...) Logger::GetInstance().Log(__VA_ARGS__)

class Logger {
    // Возвращает текущее время, либо ручное, если оно установлено
    auto GetTime() const {
        if (manual_ts_) {
            return *manual_ts_;
        }
        return std::chrono::system_clock::now();
    }

    // Возвращает отформатированную метку времени для лога
    auto GetTimeStamp() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        return std::put_time(std::localtime(&t_c), "%F %T");
    }

    // Возвращает строку с датой для имени файла: YYYY_MM_DD
    std::string GetFileTimeStamp() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&t_c), "%Y_%m_%d");
        return ss.str();
    }

    Logger() = default;
    Logger(const Logger&) = delete;

    // Вспомогательный метод для проверки и смены файла лога при смене даты
    void EnsureFileOpen(const std::string& file_date) {
        if (current_file_date_ != file_date) {
            if (log_file_.is_open()) {
                log_file_.close();
            }
            const std::string filename = "/var/log/sample_log_" + file_date + ".log";
            // Открываем файл в режиме append, чтобы не перезаписывать старые логи
            log_file_.open(filename, std::ios::app);
            current_file_date_ = file_date;
        }
    }

public:
    static Logger& GetInstance() {
        static Logger obj;
        return obj;
    }

    // Основной метод логирования. Принимает произвольное число аргументов.
    template<class... Ts>
    void Log(const Ts&... args) {
        // Захватываем мьютекс для потокобезопасности
        std::lock_guard<std::mutex> lock(mutex_);

        // Получаем текущую дату для имени файла
        std::string current_date = GetFileTimeStamp();

        // Проверяем, нужно ли открыть новый файл
        EnsureFileOpen(current_date);

        // Записываем метку времени
        log_file_ << GetTimeStamp() << ": ";

        // Распаковываем и записываем все аргументы
        (log_file_ << ... << args);

        // Завершаем строку
        log_file_ << std::endl;
    }

    // Установка ручной временной метки. Обрабатывает смену даты.
    void SetTimestamp(std::chrono::system_clock::time_point ts) {
        std::lock_guard<std::mutex> lock(mutex_);
        manual_ts_ = ts;

        // Проверяем, не поменялась ли дата. Если поменялась - файл должен быть переоткрыт.
        std::string new_file_date = GetFileTimeStamp();
        if (current_file_date_ != new_file_date) {
            if (log_file_.is_open()) {
                log_file_.close();
            }
            current_file_date_ = ""; // Сброс, чтобы следующий вызов Log открыл новый файл
        }
    }

private:
    std::optional<std::chrono::system_clock::time_point> manual_ts_;
    std::ofstream log_file_;
    std::mutex mutex_;
    std::string current_file_date_;
};