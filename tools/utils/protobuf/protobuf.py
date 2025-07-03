#!/usr/bin/elw python3

# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

from collections import namedtuple
from datetime import date
import sys, re, os, argparse

FILE_HEADER = '''/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*!! GENERATED USING protobuf.py                           !!*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/

/* LWIDIA_COPYRIGHT_BEGIN                                                 */
/*                                                                        */
/* Copyright 2020-{} by LWPU Corporation.  All rights reserved.  All       */
/* information contained herein is proprietary and confidential to LWPU */
/* Corporation.  Any use, reproduction, or disclosure without the written */
/* permission of LWPU Corporation is prohibited.                        */
/*                                                                        */
/* LWIDIA_COPYRIGHT_END                                                   */

'''.format(date.today().year)

_COMMENT = "//"
_MODSKW_PUBLIC = "public"
_TOKEN_NEWLINE = "\\n"

proto_re = re.compile(r'[a-zA-Z_][\w.]*|\d+|[=;{}]|"[^"]*"')
modskw_re = re.compile(r'\s*//\s*mods_keyword\s*(.*)')

def main():
    argparser = argparse.ArgumentParser(formatter_class= argparse.ArgumentDefaultsHelpFormatter,
                    description='''protobuf.py generate files for reading and writing protobuf
                                   messages to a bytestream.''')

    argparser.add_argument('--reader_namespace', type=str, required=False,
                           help='Namespace for the protobuf reader definitions')
    argparser.add_argument('--reader_filename', type=str, required=False,
                           help='Filename for the protobuf reader definitions')
    argparser.add_argument('--writer_namespace', type=str, required=False,
                           help='Namespace for the protobuf writer definitions')
    argparser.add_argument('--writer_filename', type=str, required=False,
                           help='Filename for the protobuf writer definitions')
    argparser.add_argument('--struct_namespace', type=str, required=False,
                           help='Namespace for the protobuf struct definitions')
    argparser.add_argument('--struct_filename', type=str, required=False,
                           help='Filename for the protobuf struct definitions')
    argparser.add_argument('--header_filename', type=str, required=False,
                           help='Filename for output of the proto file processing')
    argparser.add_argument('--handler_namespace', type=str, required=False,
                           help='Namespace for the protobuf handler definitions')
    argparser.add_argument('--handler_basename', type=str, required=False,
                           help='Base filename for the protobuf handler definitions')
    argparser.add_argument('inputfile', type=str, help='Input proto file')
    args = argparser.parse_args()

    opened = False

    if args.handler_basename and not (args.struct_filename or args.reader_filename):
        _FatalError("Handler generation requires both structs and reader")

    f = open(args.inputfile, "r")
    opened = True

    # Parse the .proto file - the result of parsing is Abstract Syntax Tree
    lexer = Lexer(f)
    root = ASTNode(ASTNode.root, "", None)
    Parse(lexer, root)

    if opened:
        f.close()

    # Rearrange messages (structures) so that they are usable in C/C++
    root.MarkUse()
    root.Rearrange()

    # Generate C/C++ macros
    headerfile = args.header_filename
    if not headerfile:
        headerfile = os.path.splitext(args.inputfile)[0] + '.h'
    with open(headerfile, "w") as f:
        EmitCode(f, root)

    protofile_base = os.path.basename(args.inputfile).split('.')[0]

    # Emit the C++ header for reading protobuf messages
    reader_namespace = args.reader_namespace
    if args.reader_filename:
        if not reader_namespace:
            reader_namespace = protofile_base.capitalize() + 'Reader'
        EmitHeader(reader_namespace, args.reader_filename, headerfile, 'reader')

    # Emit the C++ header for writing protobuf messages
    if args.writer_filename:
        namespace = args.writer_namespace
        if not namespace:
            namespace = protofile_base.capitalize() + 'Writer'
        EmitHeader(namespace, args.writer_filename, headerfile, 'writer')

    # Emit the C++ header containing structures for storing the protobuf messages
    struct_namespace = args.struct_namespace
    if args.struct_filename:
        if not struct_namespace:
            struct_namespace = protofile_base.capitalize() + 'Reader'
        EmitHeader(struct_namespace, args.struct_filename, headerfile, 'struct')

    if args.handler_basename:
        namespace = args.handler_namespace
        if not namespace:
            namespace = protofile_base.capitalize() + 'Handler'
        EmitHandlerHeader(root, struct_namespace, args.struct_filename, args.handler_basename, namespace)
        EmitHandlerCpp(root, args.reader_filename, reader_namespace, struct_namespace, args.struct_filename, args.handler_basename, namespace)

def _FatalError(msg: str, source: str = None, line: int = None) -> None:
    s = msg
    if source != None or line != None:
        assert source != None and line != None
        s = "{}:{}: {}".format(source, line, msg)
    print(s, file=sys.stderr)
    sys.exit(1)

def _StripTrailingComment(s: str) -> str:
    """Strips trailing comment from the given string, if it exists."""
    comment_i = s.find(_COMMENT)
    if comment_i > -1:
        s = s[:comment_i]
    return s

def Tokenize(f):
    """Generator, which yields subsequent tokens in the form of: (line_no, token_str)"""
    line_no = 0
    for line in f.readlines():
        line_no += 1

        # Allow for MODS specific syntax implemented through comments
        modskw = modskw_re.search(line)
        if modskw:
            if modskw.start() == 0:
                # Whole line attribute
                line = modskw.group(1).strip()
            else:
                # Trailing keyword
                kw_comment_i = modskw.group().find(_COMMENT)
                assert kw_comment_i > -1 # regex matches start with a comment

                # Remove keyword prefix from token stream
                line = line[:modskw.start()].strip() + " " + modskw.group(1).strip()

                # Remove trailing comment
                #
                # From source text, would be of the form:
                #   ... // mods_keyword ...; // comment
                #                            ^^^^^^^^^^
                line = _StripTrailingComment(line)
        else:
            line = _StripTrailingComment(line)

        # Remove leading/trailing whitespace
        line = line.strip()

        # Skip empty lines
        if len(line) == 0:
            continue

        for token in proto_re.findall(line):
            yield (line_no, token)

        # Always yield a newline token to disambiguate trailing keywords from whole line keywords
        # ex.
        #       int32 a = 1; // mods_keyword foo;
        #
        #       int32 a = 1;
        #       // mods_keyword foo;
        #
        # Both yield the token stream [int32, a, =, 1, ;, foo, ;] without newline tokens
        yield (line_no, _TOKEN_NEWLINE)

class Lexer:
    """Lexer (a.k.a. scanner).  Used for peeking and retrieving subsequent tokens."""
    def __init__(self, f):
        self._source    = f.name
        self._tokens    = Tokenize(f)
        self._next      = None
        self._line_no   = 0

    def Peek(self):
        if self._next == None:
            try:
                line_no, token = next(self._tokens)
                self._next     = token
                self._line_no  = line_no
            except StopIteration:
                self._next = ""
        return self._next

    def Next(self):
        token = self.Peek()
        self._next = None
        return token

    def Line(self) -> int:
        return self._line_no

    def Source(self) -> str:
        return self._source

FieldDesc = namedtuple('FieldDesc', ['source_file', 'line_no', 'repeated', 'type', 'name', 'index',
        'is_public'     # Is safe to be publicly visible
    ])

class ASTNode:
    """Node of an Abtract Syntax Tree representing the parsed .proto file."""
    # Type of an AST node
    root    = 0
    message = 1
    enum    = 2
    # These are the only primitive protobuf types we recognize/support
    primitive_types = ("bool", "float", "uint32", "uint64", "sint32", "sint64", "string", "bytes")

    def __init__(self, type, name, parent_node):
        self._type    = type
        self._name    = name
        self._parent  = parent_node
        self._is_container = False
        self._is_handler = False
        if type == self.root:
            self._structs = []
            self._enums   = []
        elif type == self.message:
            self._fields         = []
            self._structs        = []
            self._enums          = []
            self._siblings_using = []
            parent_node._structs.append(self)
        else:
            self._values = []
            parent_node._enums.append(self)

    def AddEnumValue(self, key, value):
        self._values.append((key, value))

    def AddField(self, fieldDesc: FieldDesc):
        self._fields.append(fieldDesc)

    def MarkUse(self):
        if self._type == self.message:
            for f in self._fields:
                self.MarkTypeUse(f.source_file, f.line_no, f.type)
        for s in self._structs:
            s.MarkUse()

    def MarkTypeUse(self, source_file, line_no, type):
        """For each structure, find out which other sibling structures are using it."""
        if type in self.primitive_types:
            return

        found = None
        node  = self
        prev  = None
        type = type.split("::")[0]
        while node != None:
            for s in node._structs:
                if s._name == type:
                    found = s
                    break
            if not found:
                for e in node._enums:
                    if e._name == type:
                        found = e
                        break
            if found:
                break
            prev = node._name
            node = node._parent
        if found:
            if prev and found._type == self.message:
                found._siblings_using.append(prev)
        else:
            _FatalError("unrecognized type '{}'".format(type), source_file, line_no)

    def Rearrange(self):
        """Rearrange structures, so that sibling structures using it are declared after it."""
        for s in self._structs:
            s.Rearrange()

        total = len(self._structs)

        while True:
            src  = None
            dest = None

            for i in range(total):
                name = self._structs[i]._name
                largest_ref = i
                for j in range(i + 1, total):
                    if name in self._structs[j]._siblings_using:
                        largest_ref = j
                if largest_ref > i:
                    src  = i
                    dest = largest_ref
                    break

            if dest:
                self._structs.insert(dest, self._structs.pop(src))
            else:
                break

def Expect(lexer, *values):
    """Expect the lexer to yield specific tokens in the specified sequence."""
    ret = ()
    for v in values:
        token = lexer.Next()
        if v == None:
            ret += (token,)
        elif token != v:
            _FatalError("'{}' expected, but found '{}'".format(v, token), lexer.Source(), lexer.Line())
    return ret

def Parse(lexer, node):
    """Parse a proto file and produce Abstract Syntax Tree."""
    Expect(lexer, "syntax", "=", '"proto3"', ";")

    while True:
        token = lexer.Peek()
        if token == "enum":
            ParseEnum(lexer, node)
        elif token == "":
            break
        elif token == _TOKEN_NEWLINE:
            # Consume newline token
            lexer.Next()
        else:
            ParseMessage(lexer, node)

def ConsumeNewline(lexer):
    """Consume a newline from the token stream, if it exists.
    """
    if lexer.Peek() == _TOKEN_NEWLINE:
        lexer.Next()

def ParseMessage(lexer, node):
    name, = Expect(lexer, "message", None)
    new_node = ASTNode(ASTNode.message, name, node)
    ConsumeNewline(lexer)
    Expect(lexer, "{")
    while True:
        token = lexer.Peek()
        if token == "message":
            ParseMessage(lexer, new_node)
        elif token == "enum":
            ParseEnum(lexer, new_node)
        elif token == "reserved":
            ParseReserved(lexer)
        elif token in ("", "}"):
            break
        elif token == _TOKEN_NEWLINE:
            # Consume newline token
            lexer.Next()
        else:
            ParseField(lexer, new_node)
    Expect(lexer, "}")

def ParseEnum(lexer, node):
    name, = Expect(lexer, "enum", None)
    new_node = ASTNode(ASTNode.enum, name, node)
    ConsumeNewline(lexer)
    Expect(lexer, "{")
    while True:
        token = lexer.Peek()
        if token in ("", "}"):
            break
        elif token == _TOKEN_NEWLINE:
            # Consume newline token
            lexer.Next()
            continue
        key, value = Expect(lexer, None, "=", None, ";")
        new_node.AddEnumValue(key, value)
    Expect(lexer, "}")

def ParseReserved(lexer):
    min, = Expect(lexer, "reserved", None)
    max = min
    if lexer.Peek() == "to":
        lexer.Next()
        max = lexer.Next()
    Expect(lexer, ";")

def ParseField(lexer, node):
    type     = lexer.Next()
    source   = lexer.Source()
    line_no  = lexer.Line()
    repeated = type == "repeated"

    if repeated:
        type = lexer.Next()
    if type == "container":
        node._is_container = True
        Expect(lexer, ";")
    elif type == "handler":
        node._is_handler = True
        Expect(lexer, ";")
    elif type == _MODSKW_PUBLIC:
        _FatalError("'public' keyword must be used as a trailing field attribute", source, line_no)
    else:
        type = type.replace('.', "::")
        name, index = Expect(lexer, None, "=", None, ";")
        # Check for field attribute
        # source text format: "// mods_keyword <attribute>;"
        is_public = False
        if lexer.Peek() == _MODSKW_PUBLIC:
            is_public = True
            Expect(lexer, _MODSKW_PUBLIC, ";")

        node.AddField(FieldDesc(source, line_no, repeated, type, name, index, is_public))

def _CppBool(b):
    return "true" if b else "false"

def EmitCode(f, node: ASTNode, indent_level: int = -1):
    """Produce macro ilwocations for use in C/C++ sources.

    @param f File to write to.
    @param node Root of AST to generate from.
    @param indent_level Indentation depth. -1 is the top level.
    """
    indent_base = " " * 4
    indent = indent_base * indent_level
    if node._type == ASTNode.enum:
        f.write("{}BEGIN_ENUM({})\n".format(indent, node._name))
        for k, v in node._values:
            f.write("{}{}DEFINE_ENUM_VALUE({}, {})\n".format(indent, indent_base, k, v))
        f.write("{}END_ENUM\n".format(indent))
    else:
        if node._type == ASTNode.message:
            f.write("{}BEGIN_MESSAGE({})\n".format(indent, node._name))
        for e in node._enums:
            EmitCode(f, e, indent_level + 1)
        for s in node._structs:
            EmitCode(f, s, indent_level + 1)
        if node._type == ASTNode.message:
            for field in node._fields:
                f_type = field.type
                if f_type in ASTNode.primitive_types:
                    f_type = "pb_" + field.type
                f.write("{}{}DEFINE_{}FIELD({}, {}, {}, {})\n".format(
                     indent, indent_base, "REPEATED_" if field.repeated else "", field.name, f_type, field.index, _CppBool(field.is_public)))
            f.write("{}END_MESSAGE\n".format(indent))

def EmitHeader(namespace, output_file, header_file, hdr_type):
    with open(output_file, "w") as f:
        f.write(FILE_HEADER)
        f.write("#pragma once\n\n")
        f.write("namespace " + namespace + "\n")
        f.write("{\n")
        f.write("    #include \"protobuf/pb" + hdr_type + "_begin.h\"\n")
        f.write("    #include \"" + os.path.basename(header_file) + "\"\n")
        f.write("    #include \"protobuf/pbcommon_end.h\"\n")
        f.write("}\n")

def GetMaxEnumValue(node, type):
    for e in node._enums:
        if e._name == type:
            values = [int(lw[1]) for lw in e._values]
            return max(values)
    if node._parent:
        return GetMaxEnumValue(node._parent, type)
    return None

def IsStructHandler(node, type):
    for e in node._structs:
        if e._name == type:
            return e._is_handler
    if node._parent:
        return IsStructHandler(node._parent, type)
    return False

def GetFullyQualifiedStructName(node):
    tempNode = node
    name = node._name
    while (tempNode._parent and tempNode._parent._type != ASTNode.root):
        name = "{}::{}".format(tempNode._parent._name, name)
        tempNode = tempNode._parent
    return name

def GetFullyQualifiedFieldType(node, type):
    # Support field types that are defined in global scope or current scope

    # If field type is a struct defined at local scope of current node,
    # prepend fully qualified name of node to field type
    for e in node._structs:
        if e._name == type:
            return "{}::{}".format(GetFullyQualifiedStructName(node), type)
    return type

def EmitHandlerFunctions(f, node, reader_namespace, struct_namespace, handler_namespace):
    """Produce macro ilwocations for use in C/C++ sources."""
    for s in node._structs:
        EmitHandlerFunctions(f, s, reader_namespace, struct_namespace, handler_namespace)
    if node._type == ASTNode.message:
        if node._is_container:
            f.write("LwDiagUtils::EC {}::Handle{}(const ByteStream & messageData, void *pvContext)\n".format(handler_namespace, node._name))
            f.write("{\n")
            f.write("    ProtobufReader::PBInput input(nullptr, &messageData, 0, messageData.size(), LwDiagUtils::PriError);\n")

            f.write("    //ProtobufReader::GetPBFieldHdr(input);\n")
            f.write("    //input.GetPBSize(input.pos);\n")
            f.write("    for (const auto& hdr : input)\n")
            f.write("    {\n")
            f.write("        switch (hdr.index)\n")
            f.write("        {\n")
        else:
            if node._is_handler:
                f.write("static LwDiagUtils::EC Handle{}(ProtobufReader::PBInput & input, const ProtobufReader::FieldHdr & hdr, void *pvContext)\n".format(node._name))
                f.write("{\n")
                f.write("    {}::{} msg = {};\n".format(struct_namespace, node._name, "{ }"))
            else:
                # Support messages that are defined in global scope or current scope
                f.write(
                    "static LwDiagUtils::EC Parse{}(ProtobufReader::PBInput & input, const ProtobufReader::FieldHdr & hdr, {}::{} & msg)\n".format(
                    node._name, struct_namespace, GetFullyQualifiedStructName(node)))
                f.write("{\n")
            f.write("    BEGIN_STRUCTURED_MEMBER(entryInput, input);\n")
        for field in node._fields:
            if field.type in ASTNode.primitive_types:
                f_type = "pb_" + field.type
                # Support messages that are defined in global scope or current scope
                f.write("        DECLARE_{}MEMBER_FIELD({}, {}, {}, {}::{})\n".format(
                        "REPEATED_" if field.repeated else "", reader_namespace,
                        GetFullyQualifiedStructName(node), field.name, struct_namespace, f_type))
            else:
                max_enum = GetMaxEnumValue(node, field.type)
                if max_enum is not None:
                    enum_type = 'UINT08'
                    if max_enum > 65535:
                        enum_type = 'UINT32'
                    elif max_enum > 255:
                        enum_type = 'UINT16'
                    f.write("        DECLARE_{}MEMBER_FIELD({}, {}, {}, {})\n".format(
                            "REPEATED_" if field.repeated else "", reader_namespace, node._name, field.name, enum_type))
                else:
                    f.write("        case {}::{}::{}:\n".format(reader_namespace, GetFullyQualifiedStructName(node), field.name))
                    f.write("        {\n")
                    f.write("            LwDiagUtils::EC ec = LwDiagUtils::OK;\n")
                    if IsStructHandler(node, field.type):
                        f.write("            CHECK_EC(::Handle{}({}, hdr, pvContext));\n".format(field.type, "input" if node._is_container else "entryInput"))
                    elif field.repeated:
                        # With repeated there are multiple instances of inner messages. So for each oclwrence, parse the
                        # value and push it to field.name vector
                        field_instance = "l_{}".format(field.type.lower())
                        f.write("            {}::{} {} = {{}};\n".format(struct_namespace,
                            GetFullyQualifiedFieldType(node,field.type), field_instance))
                        f.write("            CHECK_EC(Parse{}(entryInput, hdr, {}));\n".format(field.type, field_instance))
                        f.write("            msg.{}.push_back({});\n".format(field.name, field_instance))
                    else:
                        f.write("            CHECK_EC(Parse{}(entryInput, hdr, msg.{}));\n".format(field.type, field.name))
                    f.write("        }\n")
                    f.write("            break;\n")
        if node._is_container:
            f.write("            default:\n")
            f.write("                break;\n")
            f.write("        }\n")
            f.write("    }\n")
            f.write("    return LwDiagUtils::OK;\n")
        else:
            f.write("    END_STRUCTURED_MEMBER(entryInput, {}::{}, false);\n".format(reader_namespace, GetFullyQualifiedStructName(node)))
            if node._is_handler:
                f.write("    return {}::Handle{}(msg, pvContext);\n".format(handler_namespace, node._name))
            else:
                f.write("    return LwDiagUtils::OK;\n")
        f.write("}\n\n")

def EmitHandlerCpp(node, reader_filename, reader_namespace, struct_namespace, struct_filename, handler_basename, handler_namespace):
    with open(handler_basename + '.cpp', "w") as f:
        f.write(FILE_HEADER)
        f.write("#include \"" + os.path.basename(handler_basename + '.h') + "\"\n")
        f.write("#include \"lwdiagutils.h\"\n")
        f.write("#include \"inc/bytestream.h\"\n")
        f.write("#include \"protobuf/pbreader.h\"\n")
        f.write("#include \"protobuf/pbreader_parse.h\"\n")
        f.write("#include \"" + os.path.basename(struct_filename) + "\"\n")
        f.write("#include \"" + os.path.basename(reader_filename) + "\"\n\n")
        EmitHandlerFunctions(f, node, reader_namespace, struct_namespace, handler_namespace)

def EmitHandlerDefs(f, node, struct_namespace):
    for s in node._structs:
        EmitHandlerDefs(f, s, struct_namespace)
    if node._type == ASTNode.message:
        if node._is_handler:
            f.write("    LwDiagUtils::EC Handle{}(const {}::{} & message, void * pvContext);\n".format(node._name, struct_namespace, node._name))
        if node._is_container:
            f.write("    LwDiagUtils::EC Handle{}(const ByteStream & messageData, void * pvContext);\n".format(node._name))

def EmitHandlerHeader(node, struct_namespace, struct_filename, handler_basename, handler_namespace):
    with open(handler_basename + '.h', "w") as f:
        f.write(FILE_HEADER)
        f.write("#pragma once\n\n")
        f.write("#include \"lwdiagutils.h\"\n")
        f.write("#include \"inc/bytestream.h\"\n")
        f.write("#include \"" + os.path.basename(struct_filename) + "\"\n")
        f.write("namespace " + handler_namespace + "\n")
        f.write("{\n")
        EmitHandlerDefs(f, node, struct_namespace)
        f.write("}\n")

if __name__ == "__main__":
    main()
