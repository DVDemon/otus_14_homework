#include <string>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <future>
#include <random>
#include <mutex>

#include "concurrent.h"


bool isPositiveInteger(const std::string &&s)
{
    return !s.empty() &&
           (std::count_if(s.begin(), s.end(), [](auto a) { return (a >= '0' && a <= '9'); }) == (long)s.size());
}

/**
 * Вспомогательная функция для генерации тестовых файлов больших размеров
 */
void generate()
{
    std::ofstream file("test.txt");
    if (file.is_open())
    {
        for (int i = 0; i < 10000; i++)
        {
            std::random_device rd;
            std::default_random_engine e1(rd());
            std::uniform_int_distribution<size_t> uniform_dist_1(10, 100);
            std::uniform_int_distribution<size_t> uniform_dist_2('A', 'z');
            size_t length = uniform_dist_1(e1);
            for (size_t j = 0; j < length; j++)
                file << (char)uniform_dist_2(e1);
            file << '\n';
        }
        file.close();
    }
}

/**
 * Пользователськая функция map
 */

std::multimap<std::string, int> map_strings(const char *array, size_t length)
{
    std::multimap<std::string, int> result;
    std::string str;
    for (size_t i = 0; i < length; i++)
    {
        if (array[i] != '\n')
            str += array[i];
        else
        {
            result.insert(std::pair<std::string, int>(str, 1));
            str.clear();
        }
    }
    if (!str.empty())
        result.insert(std::pair<std::string, int>(str, 1));
    return result;
}

/**
 * Пользовательская функция reduce
 */ 
size_t reduce_strings(std::multimap<std::string, int> & input)
{
    size_t result{};
    std::string last{};
    std::string key;
    size_t      value;
    
    for(auto pair : input){
        std::tie(key,value) = pair;
        if(!last.empty()){
            result = std::max(result,
                              homework::string_difference(key,last));
        }
        last = key;
    }
    return result;
}

auto main(int argc, char *argv[]) -> int
{
    std::ifstream file;
    std::mutex file_mutex;
    //generate();
    try
    {
        if (argc == 4)
            if (isPositiveInteger(std::string(argv[2])))
                if (isPositiveInteger(std::string(argv[3])))
                {
                    size_t map_threads = atoi(argv[2]);
                    size_t reduce_threads = atoi(argv[3]);

                    if (map_threads == 0)
                    {
                        std::cout << "Map threads should be greater then zero" << std::endl;
                        return 0;
                    }

                    if (reduce_threads == 0)
                    {
                        std::cout << "Reduce threads should be greater then zero" << std::endl;
                        return 0;
                    }

                    file.open(argv[1], std::ios::binary | std::ios::ate);
                    if (file.is_open())
                    {
                        size_t file_size = file.tellg();
                        file.seekg(0);
                        std::vector<homework::MapTask> map_tasks;
                        homework::split(map_threads, file, file_size, map_tasks, &file_mutex);
                        file.close();
                        file.open(argv[1], std::ios::binary);
                        if (file.is_open())
                        {
                            auto result = homework::concurrent(reduce_threads,
                                                               map_tasks,
                                                               map_strings,
                                                               reduce_strings);
                            std::cout << result.get() << std::endl;
                            file.close();
                        }
                        return 1;
                    }
                }

        std::cerr << "Usage: yamr <src> <mnum> <rnum>\n"
                  << "src  – путь к файлу с данными.\n"
                  << "mnum – количество потоков для работы отображения\n"
                  << "rnum – количество потоков для работы свертки\n";
    }
    catch (const std::exception &ex)
    {
        if (file.is_open())
            file.close();
        std::cerr << "Exception: " << ex.what() << "\n";
    }

    return 0;
}