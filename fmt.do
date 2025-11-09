git ls-files "*.[ch]" | xargs -I{} clang-format -style="{BasedOnStyle: Google, IndentWidth: 4}" -i {}
