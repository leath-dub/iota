syntax_test(
    name="if_statement_basic",
    source="if foo {}",
    expected="""
    stmt {
      if_stmt {
        cond {
          expr {
            atom {
              scoped_ident {
                'foo'
              }
            }
          }
        }
        comp_stmt {}
      }
    }
    """,
    node="STMT",
)

syntax_test(
    name="while_statement_basic",
    source="while foo {}",
    expected="""
    stmt {
      while_stmt {
        cond {
          expr {
            atom {
              scoped_ident {
                'foo'
              }
            }
          }
        }
        comp_stmt {}
      }
    }
    """,
    node="STMT",
)

syntax_test(
    name="return_statement_basic",
    source="return x + 12;",
    expected="""
    stmt {
      return_stmt {
        expr {
          bin_expr {
            op='+'
            left:
              expr {
                atom {
                  scoped_ident {
                    'x'
                  }
                }
              }
            right:
              expr {
                atom {
                  '12'
                }
              }
          }
        }
      }
    }
    """,
    node="STMT",
)

syntax_test(
    name="case_statement_basic",
    source="""
    case x {
        let Just(_) -> do();
        ::x -> return 10;
        'x' -> {}
        10 -> {}
        "foo bar" -> {}
        else -> {}
    }
    """,
    expected="""
    stmt {
      case_stmt {
        expr {
          atom {
            scoped_ident {
              'x'
            }
          }
        }
        case_branches {
          case_branch {
            case_patt {
              unpack_union {
                tag='Just'
                binding {
                  name='_'
                }
              }
            }
            stmt {
              expr {
                fn_call {
                  expr {
                    atom {
                      scoped_ident {
                        'do'
                      }
                    }
                  }
                  args:
                    init {}
                }
              }
            }
          }
          case_branch {
            case_patt {
              expr {
                atom {
                  scoped_ident {
                    ''
                    'x'
                  }
                }
              }
            }
            stmt {
              return_stmt {
                expr {
                  atom {
                    '10'
                  }
                }
              }
            }
          }
          case_branch {
            case_patt {
              expr {
                atom {
                  char=''x''
                }
              }
            }
            stmt {
              comp_stmt {}
            }
          }
          case_branch {
            case_patt {
              expr {
                atom {
                  '10'
                }
              }
            }
            stmt {
              comp_stmt {}
            }
          }
          case_branch {
            case_patt {
              expr {
                atom {
                  '"foo bar"'
                }
              }
            }
            stmt {
              comp_stmt {}
            }
          }
          case_branch {
            case_patt {
              'else'
            }
            stmt {
              comp_stmt {}
            }
          }
        }
      }
    }
    """,
    node="STMT",
)
