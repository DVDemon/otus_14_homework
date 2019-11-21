#ifndef CONCURRENT_H
#define CONCURRENT_H

#include <future>
#include <functional>
#include <vector>
#include <set>
#include <map>

namespace homework
{

struct MapTask
{
    size_t start;
    size_t end;
    std::ifstream *file;
    std::mutex *mtx;
};
   void split(const size_t blocks,std::ifstream &file,size_t file_size,std::vector<MapTask>& result,std::mutex *mtx);

using map_func_type = std::function<std::multimap<std::string, int>(const char *array, size_t length)>;
using reduce_func_type = std::function<size_t(std::multimap<std::string, int> &)>;

void merge_into_vector(std::vector<std::string> &array, std::string value);
void shuffle(std::multimap<std::string, int> map_output, std::vector<std::multimap<std::string, int>> &reducer_input, size_t reducers);

std::multimap<std::string, int> map(MapTask map_task, map_func_type task_proc);
size_t reduce(std::multimap<std::string, int> input, reduce_func_type task_proc);

std::future<size_t> concurrent(size_t reduce_threads,
                               std::vector<MapTask> &map_tasks,
                               map_func_type map_task,
                               reduce_func_type reduce_task);
size_t string_difference(const std::string &lhv, const std::string &rhv);
} // namespace homework
#endif