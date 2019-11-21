#include <algorithm>
#include <iostream>
#include <exception>
#include <fstream>

#include "concurrent.h"

#define UNUSED(value) (void)value

/**
 * Эта функция считывает файл и разделяет его на блоки примерно одинаковой длинны.
 * Каждый блок выравнен по концу строки.
 */
void homework::split(const size_t blocks, std::ifstream &file, size_t file_size,
                     std::vector<homework::MapTask> &result,std::mutex *mtx)
{
    if (file.is_open())
    {
        std::vector<size_t> block_positions;
        block_positions.reserve(blocks);

        for (size_t i = 1; i <= blocks; i++)
            block_positions[i-1] = (file_size*i)/blocks;

        // read to the end of line
        for(size_t i = 0; i < blocks; i++){
            size_t & pos = block_positions[i];
            file.seekg(pos);
            char sign{};
            do{
                file.read(&sign,1);
                if(file.eof()) break;
                pos++;
            }while((sign!='\n')&&(pos<file_size));
        }

        for(size_t i = 0; i < blocks; i++){
            homework::MapTask t{i==0?0:block_positions[i-1]+1,block_positions[i],&file,mtx};
            if(t.end>=t.start) result.push_back(t);
        }   
    }
}

/**
 * Основаня функция выполняющая map-reduce. Оркестрирует вызов вспомогательных процедур и является точкой синхронизации
 */
std::future<size_t> homework::concurrent(size_t reduce_threads,
                                         std::vector<homework::MapTask> &map_tasks,
                                         homework::map_func_type map_task,
                                         homework::reduce_func_type reduce_task)
{
    if (reduce_threads == 0)
        throw std::logic_error("Reduce threads should be greater then zero");

    return std::async(
        [reduce_threads, &map_tasks, map_task, reduce_task]() -> size_t {
            size_t result{};

            // start map
            std::vector<std::future<std::multimap<std::string,int>>> map_results;
            for (size_t i = 0; i < map_tasks.size(); i++)
            {
                homework::MapTask &t = map_tasks[i];

                map_results.push_back(std::async(
                    [t, map_task]() -> std::multimap<std::string,int> {
                        return homework::map(t, map_task);
                    }));
            }

            // start shuffle async
            std::vector<std::multimap<std::string, int>> reduce_input;
            std::vector<std::future<void>> shuffle_results;

            for (auto &i : map_results)
            {
                std::multimap<std::string,int> val = i.get();
                shuffle_results.push_back(std::async(
                    [val, &reduce_input, reduce_threads]() {
                        homework::shuffle(val, reduce_input, reduce_threads);
                    }));
            }

            // wait for finish map and shuffle
            for (auto &i : shuffle_results)
                i.wait();

            /*
            std::cout << "Before move -----------" << std::endl;
            for (auto &i : reduce_input) {
                std::cout << "start" << std::endl;
                for (auto s : i)
                    std::cout << "   " << s << std::endl;
                std::cout << "end" << std::endl;
            }
            //*/

            // move equal between reduce blocks
            for (size_t i = 0; i < reduce_input.size() - 1; )
            {
                if (reduce_input[i].size() > 0)
                {
                    bool moving{};
                    do
                    {
                        moving = false;
                        if (i+ 1 < reduce_input.size())
                        if (reduce_input[i + 1].size() > 0)
                            if (homework::string_difference(reduce_input[i].rbegin()->first,
                                                            reduce_input[i + 1].begin()->first) > 0)
                            {
                                reduce_input[i].insert(*reduce_input[i + 1].begin());
                                reduce_input[i + 1].erase(reduce_input[i + 1].begin());
                                moving = true;
                            }
                    } while (moving);
                } 
                
                if (i+ 1 < reduce_input.size()){
                    if (reduce_input[i + 1].size() > 0) i++;
                      else reduce_input.erase(reduce_input.begin()+i+1);
                } else i++;
            }
            /*
            std::cout << "After move -----------" << std::endl;
            for (auto &i : reduce_input) {
                std::cout << "start" << std::endl;
                for (auto s : i)
                    std::cout << "   " << s.first << std::endl;
                std::cout << "end" << std::endl;
            }

            //*/

            //reduce
            std::vector<std::future<size_t>> reduce_results;

            for (size_t i = 0; i < reduce_input.size(); i++)
            {
                std::multimap<std::string, int> &t = reduce_input[i];
                reduce_results.push_back(std::async(
                    [t, reduce_task]() -> size_t {
                        return homework::reduce(t, reduce_task);
                    }));
            }

            result = 0;
            for (auto &i : reduce_results)
                result = std::max(result,i.get());

            return result;
        });
}

/**
 * Функция маппинга. Читаем блок из файла и передаем в пользовательскую map-функцию
 */
std::multimap<std::string,int> homework::map(homework::MapTask map_task, homework::map_func_type map_func)
{
    size_t length{};
    char *array{nullptr};
    // read sync
    {
        std::lock_guard<std::mutex> lck(*map_task.mtx);

        length = map_task.end - map_task.start;
        array = new char[length];

        map_task.file->seekg(map_task.start);
        map_task.file->read(array, length);
    }

    // map
    std::multimap<std::string,int> result = map_func(array, length);

    // sort - наверное не надо :-)


    return result;
}

/**
 * Функция свертки. Выполняем пользовательскую свертку и пишем в файл.
 */
size_t homework::reduce(std::multimap<std::string, int> input, homework::reduce_func_type task_proc)
{
    static size_t reducer_index{};
    size_t result = task_proc(input);

    std::string name{"reducer_"};
    name+= std::to_string(++reducer_index);
    name+=".txt";
    std::ofstream file(name);
    file << result;
    file.close();
    return result;
}

/**
 * Страшная функция по переливки из результатов map во входные данные reduce. Идея такая:
 * - вставляем сортированно каждый результат map в vector входных параметров reducer
 * - делаем балансировку, что бы в каждом reducere было примерно одинаково значений
 * - из начала каждого контейнера reducer перемещаем значение префиксы у которых совпадают хоть на один символ со
 *   строкой из конца предыдущего контейнера - в предыдущий контейнер
 */
void homework::shuffle(std::multimap<std::string,int>map_output, std::vector<std::multimap<std::string, int>> &reducer_input, size_t reducers)
{
    static std::mutex shuffle_mtx;
    std::lock_guard<std::mutex> lck(shuffle_mtx);

    while (reducer_input.size() < reducers)
        reducer_input.push_back(std::multimap<std::string, int>());

    // insert
    for (auto pair : map_output)
    {
        std::string val = pair.first;
        // find section
        int section_index{-1};
        for (size_t i = 0; (section_index == -1) && (i < reducer_input.size()); i++)
        {
            if (reducer_input[i].size() == 0)
            {
                section_index = i;
            }
            else
            {
                if (val <= reducer_input[i].rbegin()->first)
                    section_index = i;
                else
                {
                    if (i + 1 < reducer_input.size())
                    {
                        if (reducer_input[i + 1].size() > 0)
                        {
                            if (val <= reducer_input[i + 1].begin()->first)
                                section_index = i; // next reducer greater
                        }
                        else
                            section_index = i; // next reducer empty
                    }
                    else
                        section_index = i; // last reducer
                }
            }
        }

        if (section_index == -1)
            section_index = reducer_input.size() - 1;

        // insert into section
        reducer_input[section_index].insert(pair);
    }

    // balance -----------------
    size_t items{};
    size_t size{};
    for (auto i : reducer_input)
        items += i.size();
    size = (items + reducer_input.size() - 1) / reducer_input.size();

    for (size_t i = 0; i < reducer_input.size() - 1; i++)
    {
        while (reducer_input[i].size() > size)
        {
            auto val = *reducer_input[i].rbegin();
            reducer_input[i + 1].insert(val);

            auto it = std::next(reducer_input[i].begin(), reducer_input[i].size() - 1);
            reducer_input[i].erase(it);
        }
    }
}

/**
 * Считаем длину общего префикса у строк
 */
size_t homework::string_difference(const std::string &lhv, const std::string &rhv)
{
    size_t length = std::min(lhv.size(), rhv.size());
    for (size_t i = 0; i < length; i++)
    {
        if (lhv[i] != rhv[i])
            return i;
    }

    return length;
}
