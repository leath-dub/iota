git ls-files "*.[ch]" | xargs -I{} clang-format -style=Google -i {}
