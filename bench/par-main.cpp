#include <mr-importer/workers/composer.hpp>

int main() {
  mr::Composer<std::execution::par>::addTask("/home/michael/Development/Personal/mr-importer/ABeautifulGame/ABeautifulGame.gltf");

  return 0;
}
