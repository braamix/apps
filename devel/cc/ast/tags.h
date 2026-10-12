enum {
    TAG_EOL            = 0,          // end of list
    TAG_ALIGNMENTSPEC  = 0x616c7370, // 'alsp' - for struct AlignmentSpec
    TAG_DECLARATION    = 0x6465636c, // 'decl' - for struct Declaration
    TAG_DECLORSTMT     = 0x646f7374, // 'dost' - for struct DeclOrStmt
    TAG_DECLSPEC       = 0x64737063, // 'dspc' - for struct DeclSpec
    TAG_DESIGNATOR     = 0x64657367, // 'desg' - for struct Designator
    TAG_ENUMERATOR     = 0x656e756d, // 'enum' - for struct Enumerator
    TAG_EXPR           = 0x65787072, // 'expr' - for struct Expr
    TAG_EXTERNALDECL   = 0x65786463, // 'exdc' - for struct ExternalDecl
    TAG_FIELD          = 0x66656c64, // 'feld' - for struct Field
    TAG_STATIC_ASSERT  = 0x66736173, // 'fsas' - for FIELD_STATIC_ASSERT
    TAG_FORINIT        = 0x66696e69, // 'fini' - for struct ForInit
    TAG_FUNCTIONSPEC   = 0x66737063, // 'fspc' - for struct FunctionSpec
    TAG_GENERICASSOC   = 0x67617363, // 'gasc' - for struct GenericAssoc
    TAG_INITDECLARATOR = 0x6964636c, // 'idcl' - for struct InitDeclarator
    TAG_INITIALIZER    = 0x696e6974, // 'init' - for struct Initializer
    TAG_INITITEM       = 0x6969746d, // 'iitm' - for struct InitItem
    TAG_LITERAL        = 0x6c697472, // 'litr' - for struct Literal
    TAG_PARAM          = 0x7061726d, // 'parm' - for struct Param
    TAG_PROGRAM        = 0x70726f67, // 'prog' - for struct Program
    TAG_STMT           = 0x73746d74, // 'stmt' - for struct Stmt
    TAG_TYPE           = 0x74797065, // 'type' - for struct Type
    TAG_TYPEQUALIFIER  = 0x7175616c, // 'qual' - for struct TypeQualifier
};
