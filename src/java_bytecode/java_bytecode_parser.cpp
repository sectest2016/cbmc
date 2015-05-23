/*******************************************************************\

Module:

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include <fstream>

#include <util/i2string.h>
#include <util/parser.h>

#include "java_bytecode_parser.h"
#include "bytecode_info.h"

#ifdef DEBUG
#include <iostream>
#endif

class java_bytecode_parsert:public parsert
{
public:
  java_bytecode_parsert()
  {
    get_bytecodes();
  }

  virtual bool parse();
  
  typedef java_bytecode_parse_treet::classt classt;
  typedef java_bytecode_parse_treet::membert membert;
  typedef java_bytecode_parse_treet::instructiont instructiont;
  
  java_bytecode_parse_treet parse_tree;
 
protected: 
  typedef unsigned char u1;
  typedef unsigned short u2;
  typedef unsigned int u4;
  typedef unsigned long long u8;
  
  class bytecodet
  {
  public:
    irep_idt mnemonic;
    unsigned char format;
  };
  
  std::vector<bytecodet> bytecodes;
  
  typedef std::vector<exprt> constant_poolt;
  constant_poolt constant_pool;
  
  exprt &constant(u2 index)
  {
    if(index==0 || index>=constant_pool.size())
      throw "invalid constant pool index";
    return constant_pool[index];
  }
  
  void get_bytecodes()
  {
    // pre-hash the mnemonics, so we do this only once
    bytecodes.resize(256);
    for(const bytecode_infot *p=bytecode_info; p->mnemonic!=0; p++)
    {
      assert(p->opcode<bytecodes.size());
      bytecodes[p->opcode].mnemonic=p->mnemonic;
      bytecodes[p->opcode].format=p->format;
    }
  }  

  void rClassFile();
  void rconstant_pool();
  void rinterfaces(classt &parsed_class);
  void rfields(classt &parsed_class);
  void rmethods(classt &parsed_class);
  void rmethod(classt &parsed_class);
  void rclass_attribute(classt &parsed_class);
  void rmember_attribute(membert &member);
  void rbytecode(u4 code_length, membert::instructionst &);
  
  u8 read_bytes(unsigned bytes) const
  {
    u8 result=0;
    for(unsigned i=0; i<bytes; i++)
    {
      if(!*in) throw "unexpected end of input file";
      result<<=8;
      result|=in->get();
    }
    return result;
  }

  u1 read_u1() const
  {
    return read_bytes(1);
  }
  
  inline u2 read_u2() const
  {
    return read_bytes(2);
  }
  
  u4 read_u4() const
  {
    return read_bytes(4);
  }
  
  u8 read_u8() const
  {
    return read_bytes(8);
  }
};

/*******************************************************************\

Function: java_bytecode_parsert::parse

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bool java_bytecode_parsert::parse()
{
  try
  {
    rClassFile();
  }
  
  catch(const char *message)
  {
    error() << message << eom;
    return true;
  }
  
  catch(const std::string &message)
  {
    error() << message << eom;
    return true;
  }
  
  catch(...)
  {
    error() << "parsing error" << eom;
    return true;
  }
  
  return false;
}

/*******************************************************************\

Function: java_bytecode_parsert::rClassFile

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

#define ACC_PUBLIC       0x0001
#define ACC_PRIVATE      0x0002
#define ACC_PROTECTED    0x0004
#define ACC_STATIC       0x0008
#define ACC_FINAL        0x0010
#define ACC_SYNCHRONIZED 0x0020
#define ACC_BRIDGE       0x0040
#define ACC_VARARGS      0x0080
#define ACC_NATIVE       0x0100
#define ACC_ABSTRACT     0x0400
#define ACC_STRICT       0x0800
#define ACC_SYNTHETIC    0x1000

#include <iostream>

void java_bytecode_parsert::rClassFile()
{
  u4 magic=read_u4();
  u2 minor_version=read_u2(), major_version=read_u2();
  
  if(magic!=0xCAFEBABE) throw "wrong magic";

  if(major_version<44)
    throw "unexpected major version";
  
  rconstant_pool();

  parse_tree.classes.push_back(classt());
  classt &parsed_class=parse_tree.classes.back();

  u2 access_flags=read_u2();
  u2 this_class=read_u2();
  u2 super_class=read_u2();
  
  parsed_class.name=
    constant(constant(this_class).get_unsigned_int(ID_name)).id();

  parsed_class.extends=
    constant(constant(super_class).get_unsigned_int(ID_name)).id();

  rinterfaces(parsed_class);
  rfields(parsed_class);
  rmethods(parsed_class);
  
  u2 attributes_count=read_u2();

  for(unsigned j=0; j<attributes_count; j++)
    rclass_attribute(parsed_class);
}

/*******************************************************************\

Function: java_bytecode_parsert::rconstant_pool

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

#define CONSTANT_Class                7
#define CONSTANT_Fieldref             9
#define CONSTANT_Methodref           10
#define CONSTANT_InterfaceMethodref  11
#define CONSTANT_String               8
#define CONSTANT_Integer              3
#define CONSTANT_Float                4
#define CONSTANT_Long                 5
#define CONSTANT_Double               6
#define CONSTANT_NameAndType         12
#define CONSTANT_Utf8                 1
#define CONSTANT_MethodHandle        15
#define CONSTANT_MethodType          16
#define CONSTANT_InvokeDynamic       18

#include <iostream>

void java_bytecode_parsert::rconstant_pool()
{
  u2 constant_pool_count=read_u2();
  if(constant_pool_count==0) throw "invalid constant_pool_count";
  constant_pool.resize(constant_pool_count);
  
  for(constant_poolt::iterator
      it=constant_pool.begin();
      it!=constant_pool.end();
      it++)
  {
    // the first entry isn't used
    if(it==constant_pool.begin()) continue;
    
    u1 tag=read_u1();
    switch(tag)
    {
    case CONSTANT_Class:
      it->id(ID_class);
      it->set(ID_name, read_u2());
      break;

    case CONSTANT_Fieldref:
      it->id("fieldref");
      it->set(ID_class, read_u2());
      it->set(ID_name, read_u2());
      break;
      
    case CONSTANT_Methodref:
      it->id("methodref");
      it->set(ID_class, read_u2());
      it->set(ID_name, read_u2());
      break;

    case CONSTANT_InterfaceMethodref:
      it->id("interfacemethodref");
      it->set(ID_class, read_u2());
      it->set(ID_name, read_u2());
      break;

    case CONSTANT_String:
      it->id(ID_string);
      it->set(ID_index, read_u2());
      break;

    case CONSTANT_Integer:
      it->id(ID_int);
      it->set(ID_value, read_u4());
      break;

    case CONSTANT_Float:
      it->id(ID_float);
      it->set(ID_value, read_u4());
      break;
      
    case CONSTANT_Long:
      it->id(ID_long);
      it->set(ID_value, read_u8());
      break;

    case CONSTANT_Double:
      it->id(ID_double);
      it->set(ID_value, read_u8());
      break;

    case CONSTANT_NameAndType:
      it->id("nameandtype");
      it->set(ID_name, read_u2());
      it->set(ID_type, read_u2());
      break;

    case CONSTANT_Utf8:
      {
        u2 bytes=read_u2();
        std::string s;
        s.resize(bytes);
        for(std::string::iterator s_it=s.begin(); s_it!=s.end(); s_it++)
          *s_it=read_u1();
        it->id(s);
      }
      break;

    case CONSTANT_MethodHandle:
      it->id("methodhandle");
      it->set("kind", read_u1());
      it->set(ID_index, read_u2());
      break;

    case CONSTANT_MethodType:
      it->id("methodtype");
      it->set(ID_index, read_u2());
      break;

    case CONSTANT_InvokeDynamic:
      it->id("invokedynamic");
      it->set("bootstrap_method_attr", read_u2());
      it->set("nameandtype", read_u2());
      break;

    default:
      throw std::string("unknown constant pool entry (")+
            i2string(tag)+")";
    }
  }
}

/*******************************************************************\

Function: java_bytecode_parsert::rinterfaces

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void java_bytecode_parsert::rinterfaces(classt &parsed_class)
{
  u2 interfaces_count=read_u2();

  for(unsigned i=0; i<interfaces_count; i++)
    parsed_class.implements.push_back(constant(read_u2()).id());
}

/*******************************************************************\

Function: java_bytecode_parsert::rfields

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void java_bytecode_parsert::rfields(classt &parsed_class)
{
  u2 fields_count=read_u2();

  for(unsigned i=0; i<fields_count; i++)
  {
    parsed_class.members.push_back(membert());
    membert &member=parsed_class.members.back();
    
    u2 access_flags=read_u2();
    u2 name_index=read_u2();
    u2 descriptor_index=read_u2();
    u2 attributes_count=read_u2();
    
    member.is_method=false;
    member.name=constant(name_index).id();
    member.signature=id2string(constant(descriptor_index).id());

    for(unsigned j=0; j<attributes_count; j++)
      rmember_attribute(member);
  }
}

/*******************************************************************\

Function: java_bytecode_parsert::rbytecode

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

#include <iostream>

void java_bytecode_parsert::rbytecode(
  u4 code_length,
  membert::instructionst &instructions)
{
  std::vector<u1> code;
  code.resize(code_length);

  for(std::vector<u1>::iterator it=code.begin();
      it!=code.end(); it++)
    *it=read_u1();

  for(unsigned address=0; address<code.size(); address++)
  {
    u1 bytecode=code[address];
    
    instructions.push_back(instructiont());
    instructiont &instruction=instructions.back();
    instruction.statement=bytecodes[bytecode].mnemonic;
    instruction.address=address;
    
    std::cout << "CODE: " << address << " " << instruction.statement << " "
              << (int)bytecodes[bytecode].format << "\n";

    switch(bytecodes[bytecode].format)
    {
    case 0: // no further bytes
      break;

    case 1: // one further byte
      {
        u1 arg=code[address];
        address+=1;
      }
      break;
    
    case 2: // two further bytes
      {
        u2 arg=(code[address]<<8)|code[address+1];
        address+=2;
      }
      break;
    
    case 4: // four further bytes
      {
        u4 arg=(code[address+0]<<24)|(code[address+1]<<16)|
               (code[address+2]<<8) |code[address+3];
        address+=4;
      }
      break;
    
    default:;
    }
  }
}

/*******************************************************************\

Function: java_bytecode_parsert::rmember_attribute

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void java_bytecode_parsert::rmember_attribute(membert &member)
{
  u2 attribute_name_index=read_u2();
  u4 attribute_length=read_u4();
  
  irep_idt attribute_name=constant(attribute_name_index).id();
  
  if(attribute_name=="Code")
  {
    u2 max_stack=read_u2();
    u2 max_locals=read_u2();
    u4 code_length=read_u4();

    rbytecode(code_length, member.instructions);

    u2 exception_table_length=read_u2();

    for(unsigned e=0; e<exception_table_length; e++)
    {
      u2 start_pc=read_u2();
      u2 end_pc=read_u2();
      u2 handler_pc=read_u2();
      u2 catch_type=read_u2();
    }

    u2 attributes_count=read_u2();

    for(unsigned j=0; j<attributes_count; j++)
      rmember_attribute(member);
  }
  else
  {
    // unknown
    std::vector<u1> info;
    info.resize(attribute_length);
    for(std::vector<u1>::iterator it=info.begin();
        it!=info.end();
        it++)
      *it=read_u1();
  }
}

/*******************************************************************\

Function: java_bytecode_parsert::rclass_attribute

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void java_bytecode_parsert::rclass_attribute(classt &parsed_class)
{
  u2 attribute_name_index=read_u2();
  u4 attribute_length=read_u4();
  std::vector<u1> info;
  info.resize(attribute_length);
  for(std::vector<u1>::iterator it=info.begin();
      it!=info.end();
      it++)
    *it=read_u1();
}

/*******************************************************************\

Function: java_bytecode_parsert::rmethods

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void java_bytecode_parsert::rmethods(classt &parsed_class)
{
  u2 methods_count=read_u2();

  for(unsigned j=0; j<methods_count; j++)
    rmethod(parsed_class);
}

/*******************************************************************\

Function: java_bytecode_parsert::rmethod

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

#define ACC_PUBLIC     0x0001
#define ACC_FINAL      0x0010
#define ACC_SUPER      0x0020
#define ACC_INTERFACE  0x0200
#define ACC_ABSTRACT   0x0400
#define ACC_SYNTHETIC  0x1000
#define ACC_ANNOTATION 0x2000
#define ACC_ENUM       0x4000

#include <iostream>

void java_bytecode_parsert::rmethod(classt &parsed_class)
{
  parsed_class.members.push_back(membert());
  membert &member=parsed_class.members.back();

  u2 access_flags=read_u2();
  u2 name_index=read_u2();
  u2 descriptor_index=read_u2();
  
  member.is_method=true;
  member.name=constant(name_index).id();
  member.signature=id2string(constant(descriptor_index).id());
  
  u2 attributes_count=read_u2();

  for(unsigned j=0; j<attributes_count; j++)
    rmember_attribute(member);
}

/*******************************************************************\

Function: java_bytecode_parse

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bool java_bytecode_parse(
  const std::string &file,
  java_bytecode_parse_treet &parse_tree,
  message_handlert &message_handler)
{
  std::ifstream in(file.c_str());

  if(!in)
  {
    messaget message(message_handler);
    message.error() << "failed to open input file" << messaget::eom;
    return true;
  }

  java_bytecode_parsert java_bytecode_parser;
  java_bytecode_parser.in=&in;
  java_bytecode_parser.set_message_handler(message_handler);
  
  bool parser_result=java_bytecode_parser.parse();

  parse_tree.swap(java_bytecode_parser.parse_tree);

  return parser_result;
}