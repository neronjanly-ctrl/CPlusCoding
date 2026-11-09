#include <iostream>

int calculate(int value) {
  int result = value * 2;

  result += 1;

  result *= 2;

  std::cout << "result = " << result << '\n';

  return result;
}

int main() {
  int value = 21;

  std::cout << "Program started\n";

  calculate(value);

  int *ptr = nullptr;

  std::cout << "About to crash\n";

  *ptr = 100;

  std::cout << "This line should not execute\n";

  return 0;
}
