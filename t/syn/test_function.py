from dataclasses import dataclass
from string import Template
from typing import Callable
import textwrap

# This is maybe overkill but it is a deep coverage test of various
# permutations of different function declarations

funf = Template("fun $name($params) $return_type{}")
paramf = Template("$name $param_type")
multi_paramf = Template("$name.. $param_type")

expectedf = Template("""
    (imports)
    (declarations
      (declaration
        (function_declaration
          name: $name
$body
          (compound_statement))))
   """)

expected_paramf = Template("""(function_parameter
  name: (variable_binding
    $name)
  variadic: $variadic
  (type
    (builtin_type
      $param_type)))""")

@dataclass
class TestCase:
    source: str
    expected: str

@dataclass
class FunDef:
    name: str
    params: int
    trailing_comma: bool
    return_type: str

    def for_each_param(self, do: Callable[[bool], None]):
        for bit in range(0, 5):
            do(self.params >> bit == 0)

    def generate_source(self) -> str:
        def generate_param(is_variadic: bool) -> str:
            template = multi_paramf if is_variadic else paramf
            return template.substitute(name="x", param_type="s32")

        params: list[str] = []
        self.for_each_param(lambda is_variadic: params.append(generate_param(is_variadic)))

        params_txt = ", ".join(params)
        if self.trailing_comma:
            params_txt += ","

        return_type_txt = self.return_type
        if len(self.return_type) != 0:
            return_type_txt += " "
        return funf.substitute(name=self.name, params=params_txt, return_type=return_type_txt)

    def generate_expected(self) -> str:
        def generate_param(is_variadic: bool) -> str:
            return expected_paramf.substitute(
                name="x",
                variadic=str(is_variadic).lower(),
                param_type="s32",
            )

        params: list[str] = []
        self.for_each_param(lambda is_variadic: params.append(generate_param(is_variadic)))

        params_txt = textwrap.indent("\n".join(params), "  ")
        params_txt = f"(function_parameter_list\n{params_txt})"

        return_type = self.return_type
        if len(self.return_type) != 0:
            return_type = f"(type\n  (builtin_type\n    {self.return_type}))"

        body = "\n".join((params_txt, return_type))
        body = textwrap.indent(body, "  " * 5).rstrip()

        return expectedf.substitute(name=self.name, body=body)

    def generate(self) -> TestCase:
        return TestCase(
            source=self.generate_source(),
            expected=self.generate_expected(),
        )

n = 0
for params in range(0, 5):
    for trailing_comma in [True, False]:
        for return_type in ["", "s32"]:
            for params in range(0, 0b11111):
                fun_def = FunDef(name="foo", params=params, trailing_comma=trailing_comma, return_type=return_type)
                test_case = fun_def.generate()
                syntax_test(  # pyright: ignore[reportUndefinedVariable]
                    name = f"function_coverage_{n}",
                    source = test_case.source,
                    expected = test_case.expected,
                )
                n += 1
