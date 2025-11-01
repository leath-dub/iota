syntax_test(
    name="if_statement_basic",
    source="if foo; {}",
    expected="""
    stmt {
      if_stmt {
        cond {
          expr {
            atom {
              designator {
                scoped_ident {
                  'foo'
                }
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
    source="while foo; {}",
    expected="""
    stmt {
      while_stmt {
        cond {
          expr {
            atom {
              designator {
                scoped_ident {
                  'foo'
                }
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
                  designator {
                    scoped_ident {
                      'x'
                    }
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
    case x; {
        Just(_) -> do();
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
            designator {
              scoped_ident {
                'x'
              }
            }
          }
        }
        case_branches {
          case_branch {
            case_patt {
              scoped_ident {
                'Just'
              }
              binding {
                name='_'
              }
            }
            stmt {
              expr {
                fn_call {
                  expr {
                    atom {
                      designator {
                        scoped_ident {
                          'do'
                        }
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
              scoped_ident {
                ''
                'x'
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
              lit=''x''
            }
            stmt {
              comp_stmt {}
            }
          }
          case_branch {
            case_patt {
              lit='10'
            }
            stmt {
              comp_stmt {}
            }
          }
          case_branch {
            case_patt {
              lit='"foo bar"'
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
