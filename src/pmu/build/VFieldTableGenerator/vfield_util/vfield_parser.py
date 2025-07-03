#!usr/bin/python3
import re
import ast
import abc
import operator
import math

class AbstractMacro:
    """
    An abstract class for Object and Function Macro
    """

    def __init__(self, name):
        self.name = name

    @abc.abstractclassmethod
    def value(self, input):
        raise NotImplementedError("Method value not implemented!")

class ObjectMacro(AbstractMacro):
    def __init__(self, name, val):
        super().__init__(name)
        self.val = val

    def value(self, input):
        return self.val.upper().replace('X', 'x')

class FunctionMacro(AbstractMacro):
    def __init__(self, name, inputArg, expr):
        super().__init__(name)
        self.inputArg = inputArg
        self.expr      = expr

    def value(self, input):
        return '0x{:08X}'.format(eval(self.expr.replace(self.inputArg, str(input)))) \

class AbstractParser:
    """
    Abstract parser that encapsulates common behaviour for all
    parsers implemented in this file.
    The AbstractParser is composed of patterns, actions corresponding
    to patterns matched and states.

    States are used to be a guard against certain actions and a way
    of detecting errors. If an action is expected to be exelwted
    when parser is in state S1, but current state is S0, then
    the parser will report an error and stop parsing.
    If state of AbstractParser is not needed, simply use None as 
    expected/current state.

    The Abstract parser will trigger action of the first matched pattern, so
    be careful not to have patterns that can be matched on the same thing.

    To implement a new parser the following steps are required:
        1. Define a new class that will extend AbstractParser
        2. Define states a parser can be found int
        2. Define regex patterns that should be matched
        3. Define actions using _actionFactory method
        4. Create a map of pattern => action and pass that to AbstractParser in
           constructor.
    """
    # ------------------- Static methods -----------------------
    @staticmethod
    def _actionFactory(expectedState, f):
        def closureAction(parser, matchedPattern):
            if parser.state == expectedState or expectedState is None:
                f(parser, matchedPattern)
            else:
                raise Exception('Unexpected state! Found {} but expected {}\n{}'.format(parser.state, expectedState, matchedPattern.group(0)))
        return closureAction

    @staticmethod
    def _actionChangeStateFactory(newState):
        def closureActionChangeState(parser, matchedPattern):
            parser.state = newState

        return closureActionChangeState
    # ----------------- End of static methods --------------------

    def __init__(self, patternActionMap):
        self.patternActionMap = patternActionMap
        self.state = None

    @abc.abstractmethod
    def _value(self):
        raise NotImplementedError("Must override method _value")

    def _resetState(self):
        self.state = None

    def parse(self, file):
        self._resetState()
        with open(file, 'r') as f:
            for s in f.readlines():
                for pattern, action in self.patternActionMap.items():
                    m = pattern.match(s)
                    if m:
                        action(self, m)
                        break

        return self._value()

class MacroParser(AbstractParser):
    # -------------------  Static methods ------------------------
    @staticmethod
    def __actionDefineParse(parser, matchedPattern):
        defineName = matchedPattern.group(1)
        defineArg  = matchedPattern.group(3)
        defineVal  = matchedPattern.group(4)

        if defineArg:
            parser.definesMap[defineName] = FunctionMacro(defineName, defineArg, defineVal)
        else:
            parser.definesMap[defineName] = ObjectMacro(defineName, defineVal)
    # ----------------- End of static methods --------------------

    # ------------------    Static fields     --------------------
    __REGX_MACRO_NAME = r'[_A-Z0-9]+'

    # group 1 => macro name
    # group 3 => macro parameters
    # group 4 => macro expression/value
    __REGX_DEFINES     = r'^ *#define +({macroName})(\(([A-Za-z])\))? +(.+?)( /\*.+)?$' \
                          .format(macroName=__REGX_MACRO_NAME)

    # Must be done like this since
    # Static method is a wrapper/decorator for function
    _ACTION_DEFINE_PARSE = AbstractParser._actionFactory(None,
                                                         __actionDefineParse.__func__)

    __REGX_ACTION_MAP = {
        re.compile(__REGX_DEFINES) : _ACTION_DEFINE_PARSE
    }
    # ------------------ End of static fields --------------------

    def __init__(self):
        super().__init__(self.__REGX_ACTION_MAP)

        self._resetState()

    def _value(self):
        return self.definesMap

    def _resetState(self):
        self.definesMap = {}
        self.state      = None


class VFieldIdParser(AbstractParser):
    # -------------------  Static methods ------------------------
    @staticmethod
    def __actiolwfieldIdParse(parser, matchedPattern):
        vfieldName = matchedPattern.group(1)
        vfieldVal  = matchedPattern.group(2)

        parser.vfieldIdsMap[vfieldName] = int(vfieldVal, base=16)
    # ----------------- End of static methods --------------------

    # ------------------    Static fields     --------------------
    __REGX_COMMENT  = r';.*'
    __REGX_HEXVAL   = r'[0-9a-fA-F]'

    # group 1 => Vfield name
    # group 2 => Vfield value
    __REGX_VFIELD_ID = r'^(VFIELD_?ID[a-zA-Z_0-9]+) + equ +({}+)h(?:{})?' \
                        .format(__REGX_HEXVAL, __REGX_COMMENT)

    __ACTION_VFIELD_ID_PARSE = AbstractParser._actionFactory(None,
                                                             __actiolwfieldIdParse.__func__)
    __REGX_ACTION_MAP = {
        re.compile(__REGX_VFIELD_ID) : __ACTION_VFIELD_ID_PARSE
    }
    # ------------------ End of static fields --------------------

    def __init__(self):
        super().__init__(self.__REGX_ACTION_MAP)

        self._resetState()

    def _value(self):
        return self.vfieldIdsMap

    def _resetState(self):
        self.vfieldIdsMap = {}
        self.state        = None

class VFieldTableParser(AbstractParser):
    # -------------------  Static methods ------------------------
    # If we matched IFDEF, add macro name to required definition
    @staticmethod
    def __actionIfDefStart(parser, matchedPattern):
        if parser.defineRequired:
            raise Exception('Error, nested ifdefs!')
        else:
            parser.defineRequired = matchedPattern.group(1)
            parser.parseVfield    = parser.defineRequired in parser.definesMap

    # Remove condition to parse VFIELD/VFIELD_REG entry
    @staticmethod
    def __actionIfDefEnd(parser, matchedPattern):
        if parser.defineRequired is None:
            raise Exception('Error, unmatched IFDEF END found!')
        else:
            parser.defineRequired = None

    # Catch VFieldReg name
    @staticmethod
    def __actiolwfieldRegEntryName(parser, matchedPattern):
        if not parser.parseVfield:
            return

        # Let's assume that the value in the beginning is 0
        parser.vfieldRegTbl[matchedPattern.group(1)] = '0x0000000'
        # We add registry name, since multiple registers can point to same addr
        parser.vfieldRegNames.append(matchedPattern.group(1))

    # Catch VFieldReg value
    @staticmethod
    def __actiolwfieldRegEntryValue(parser, matchedPattern):
        if not parser.parseVfield:
            return
        vfieldRegTbl   = parser.vfieldRegTbl
        vfieldRegNames = parser.vfieldRegNames

        value      = matchedPattern.group(1)
        macroParam = matchedPattern.group(3)

        # This means value is actually a Macro, not a const
        if value in parser.definesMap:
            # param can be a const or a macro
            macroParam = parser.definesMap.get(macroParam, macroParam)
            value      = parser.definesMap[value].value(macroParam)
        
        # All the registers so far found point to this definition
        # Save their common(same) address
        for vfieldRegName in vfieldRegNames:
            vfieldRegTbl[vfieldRegName] = value

        # They are all parsed, reset the list
        parser.vfieldRegNames.clear()

    # Catch VField name
    @staticmethod
    def __actiolwfieldEntry(parser, matchedPattern):
        if not parser.parseVfield:
            return

        vfieldTbl    = parser.vfieldTbl
        vfieldRegTbl = parser.vfieldRegTbl
        vfieldRegVal = vfieldRegTbl[matchedPattern.group(2)]

        vfieldTbl[matchedPattern.group(1)] = vfieldRegVal

    # Helper method for simple state change action
    def _actionChangeState(expectedState, newState):
        return AbstractParser._actionFactory(expectedState,
                                             AbstractParser
                                             ._actionChangeStateFactory(newState))
    # ----------------- End of static methods --------------------

    # ------------------  Static fields ------------------
    # States our parser can be found in.
    __STATE_VFIELD_REG_TBL_SEARCH = "VfieldRegTblSearch"
    __STATE_VFIELD_REG_TBL_PARSE  = "VfieldRegTblParse"
    __STATE_VFIELD_TBL_SEARCH     = "VfieldTblSearch"
    __STATE_VFIELD_TBL_PARSE      = "VfieldTblParse"
    __STATE_VFIELD_DONE           = "VfieldDone"

    # Regex used for pattern matching
    __REGX_MACRO_NAME             = r'[_A-Z0-9]+'
    __REGX_VFIELD_REG_TBL_START   = r'^ *public VFieldRegTableStart *$'
    __REGX_VFIELD_REG_TBL_END     = r'^ *VFieldRegTableEnd'
    __REGX_VFIELD_TBL_START       = r'^ *public VFieldTable *$'
    __REGX_VFIELD_TBL_END         = r'^ *VFieldTableEnd'
    __REGX_VFIELD_IFDEF_START     = r'^ *IFDEF ({macroName}) *.*$' \
                                      .format(macroName=__REGX_MACRO_NAME)
    __REGX_VFIELD_IFDEF_END       = r'^ *ENDIF *.+$'
    __REGX_VFIELD_REG_ENTRY_NAME  = r'^ *public (VFIELD_{macroName}) *.*$' \
                                      .format(macroName=__REGX_MACRO_NAME)
    __REGX_VFIELD_REG_ENTRY_VALUE = \
        r'^ *VFIELD_REG_ENTRY +\{ *\{.*\} *, *(.*?)(\(([^,]+)\))? *, +.*, +.*\} *.*$'
    __REGX_VFIELD_ENTRY           = \
        r'^ *VFIELD_ENTRY *\{ *(.+), *\{ *(?: *0 *,)? *([^,]+), *.*\} *\} *.*$'

    # Actions
    __ACTION_IFDEF_START            = AbstractParser._actionFactory(None,
                                                                    __actionIfDefStart.__func__)
    __ACTION_IFDEF_END              = AbstractParser._actionFactory(None,
                                                                    __actionIfDefEnd.__func__)
    __ACTION_VFIELD_REG_ENTRY_NAME  = AbstractParser._actionFactory(__STATE_VFIELD_REG_TBL_PARSE,
                                                                    __actiolwfieldRegEntryName
                                                                    .__func__)
    __ACTION_VFIELD_REG_ENTRY_VALUE = AbstractParser._actionFactory(__STATE_VFIELD_REG_TBL_PARSE,
                                                                    __actiolwfieldRegEntryValue
                                                                    .__func__)
    __ACTION_VFIELD_ENTRY           = AbstractParser._actionFactory(__STATE_VFIELD_TBL_PARSE,
                                                                    __actiolwfieldEntry.__func__)
    __ACTION_VFIELD_REG_TBL_START   = _actionChangeState(__STATE_VFIELD_REG_TBL_SEARCH,
                                                         __STATE_VFIELD_REG_TBL_PARSE)
    __ACTION_VFIELD_REG_TBL_END     = _actionChangeState(__STATE_VFIELD_REG_TBL_PARSE,
                                                         __STATE_VFIELD_TBL_SEARCH)
    __ACTION_VFIELD_TBL_START       = _actionChangeState(__STATE_VFIELD_TBL_SEARCH,
                                                         __STATE_VFIELD_TBL_PARSE)
    __ACTION_VFIELD_TBL_END         = _actionChangeState(__STATE_VFIELD_TBL_PARSE,
                                                         __STATE_VFIELD_DONE)

    # Map of regex to action
    __VFIELD_REGX_ACTION_MAP = {
        re.compile(__REGX_VFIELD_REG_TBL_START   ) : __ACTION_VFIELD_REG_TBL_START,
        re.compile(__REGX_VFIELD_REG_TBL_END     ) : __ACTION_VFIELD_REG_TBL_END,
        re.compile(__REGX_VFIELD_TBL_START       ) : __ACTION_VFIELD_TBL_START,
        re.compile(__REGX_VFIELD_TBL_END         ) : __ACTION_VFIELD_TBL_END,
        re.compile(__REGX_VFIELD_IFDEF_START     ) : __ACTION_IFDEF_START,
        re.compile(__REGX_VFIELD_IFDEF_END       ) : __ACTION_IFDEF_END,
        re.compile(__REGX_VFIELD_REG_ENTRY_NAME  ) : __ACTION_VFIELD_REG_ENTRY_NAME,
        re.compile(__REGX_VFIELD_REG_ENTRY_VALUE ) : __ACTION_VFIELD_REG_ENTRY_VALUE,
        re.compile(__REGX_VFIELD_ENTRY           ) : __ACTION_VFIELD_ENTRY,
    }
    # ------------------ End of static fields ------------------

    def __init__(self, definesMap, vfieldIdMap):
        super().__init__(self.__VFIELD_REGX_ACTION_MAP)
        self._resetState()
        self.definesMap  = definesMap
        self.vfieldIdMap = vfieldIdMap

    def _resetState(self):
        self.state             = self.__STATE_VFIELD_REG_TBL_SEARCH
        self.foundVfieldRegTbl = False
        self.foundVfieldTbl    = False
        self.parseVfield       = True
        self.defineRequired    = None
        self.vfieldName        = None

        # Multiple Registers can be mapped to same ADDR
        self.vfieldRegNames    = []
        self.match             = None
        self.vfieldRegTbl      = {}
        self.vfieldTbl         = {}

    def _value(self):
        return self.vfieldTbl
