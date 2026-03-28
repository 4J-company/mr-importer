#include <taskflow/taskflow.hpp>

namespace mr::importer::taskflow_exec {

tf::Executor &import_executor()
{
  static tf::Executor executor;
  return executor;
}

} // namespace mr::importer::taskflow_exec
