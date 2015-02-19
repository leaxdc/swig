/* ----------------------------------------------------------------------------- 
 * EMSCRIPTEN language module for SWIG.
 * ----------------------------------------------------------------------------- */


#include "swigmod.h"
#include <string>
#include <iostream>
#include <map>
#include <deque>

class EMSCRIPTEN: public Language {
  protected:

   /* General DOH objects used for holding the strings */
   File *f_begin;
   File *f_runtime;
   File *f_header;
   File *f_wrappers;
   File *f_init;   
   
  public:
    virtual void main(int argc, char *argv[]) {
            
      /* Set language-specific subdirectory in SWIG library */
      SWIG_library_directory("emscripten");

      /* Set language-specific preprocessing symbol */
      Preprocessor_define("SWIGEMSCRIPTEN 1", 0);

      /* Set language-specific configuration file */
      SWIG_config_file("emscripten.swg");

      /* Set typemap language (historical) */
      SWIG_typemap_lang("emscripten");
    }

    virtual int top(Node *n) {
      
       /* Get the module name */
       String *module = Getattr(n,"name");
       /* Get the output file name */
       String *outfile = Getattr(n,"outfile");

       /* Initialize I/O */
       f_begin = NewFile(outfile, "w", SWIG_output_files());
       if (!f_begin) {
          FileErrorDisplay(outfile);
          SWIG_exit(EXIT_FAILURE);
       }
      
       f_runtime = NewString("");
       f_init = NewString("");
       f_header = NewString("");
       f_wrappers = NewString("");  

       /* Register file targets with the SWIG file handler */
       Swig_register_filebyname("begin", f_begin);
       Swig_register_filebyname("header", f_header);
       Swig_register_filebyname("wrapper", f_wrappers);
       Swig_register_filebyname("runtime", f_runtime);
       Swig_register_filebyname("init", f_init);
       
       /* Output module initialization code */
       Swig_banner(f_begin);
       
       /* Emit code for children */
       Language::top(n);
       
       /* Write all to the file */
       // Disable dumping runtime
       // Dump(f_runtime, f_begin);
       Dump(f_header, f_begin);       
       Printf(f_begin, "%s(%s) { \n", "EMSCRIPTEN_BINDINGS", module);
       Dump(f_wrappers, f_begin);
       Printf(f_begin, "%s\n", "} // EMSCRIPTEN_BINDINGS");
       Wrapper_pretty_print(f_init, f_begin);
    
       /* Cleanup files */       
       Delete(f_runtime);
       Delete(f_header);
       Delete(f_wrappers);
       Delete(f_init);
       Delete(f_begin);
       
       return SWIG_OK;
    }

    virtual int  functionWrapper(Node * node) {

      /* get useful attributes */
      String   *name      = Getattr(node,"name");
      String   *symName   = Getattr(node,"sym:name");            
      String   *nodeType  =  nodeType(node);

      /* skip alien nodes */

      if (Strcmp(nodeType,"cdecl")) {
        return Language::functionWrapper(node);       
      }

      if (checkAttribute(node, "ismember", "1")) {
        return Language::functionWrapper(node);       
      }
      
      if ( Strstr(symName, "____regopt_") ) {
        String   *regOpt      = Getattr(node,"feature:emscripten_register_optional");

        Printf(f_wrappers, "\t%s\n", regOpt);
        return SWIG_OK;
      }
      
      if ( Strstr(symName, "____regvec_") ) {                   
        String   *regVec      = Getattr(node,"feature:emscripten_register_vector");

        Printf(f_wrappers, "\t%s\n", regVec);
        return SWIG_OK;
      }
      
      Printf(f_wrappers, "\tfunction(\"%s\",&%s", symName, name);

      if (checkAttribute (node, "feature:emscripten_allow_raw_pointers", "1")) {
        Printf(f_wrappers, "%s", ", emscripten::allow_raw_pointers()");
      }

      Printf(f_wrappers, "%s", ");\n");

      return SWIG_OK;
  
    }

    String *NewKind(Node *n) {

      if (checkAttribute(n, "feature:emscripten_value_object", "1") ) {
        return NewStringf("%s", "value_object");      
      } else {
        return NewStringf("%s", "class_");
      }      
    }

    void printBases(Node *node) {
      
      String   *className = Getattr(node,"name");
      String   *symName = Getattr(node,"sym:name");             

      List *baselist = Getattr(node,"bases");

      // List *bases = Swig_make_inherit_list(className, baselist, "");

      String *kind = NewKind(node);

      if (!kind) {        
        return;
      }

      if (!baselist) {

        Printf(f_wrappers, "\n\t%s<%s>(\"%s\")", kind, className, symName);
      }
      else {
        
        Printf(f_wrappers, "\n\t%s<%s, base<", kind, className);

        Iterator it = First(baselist);
        
        while (it.item)
        {          
          Printf(f_wrappers, "%s", Getattr(it.item, "name" ) );
          
          it = Next(it);
          if (it.item) {            
            Printf (f_wrappers, "%s", ", ");
          }
          else {              
            break;
          }
        }

        Printf(f_wrappers, ">>(\"%s\")", symName);
      }

      Delete(kind);      
    }

    void printSharedPtr(Node *node) {

      String   *className      = Getattr(node,"name");

      if (checkAttribute(node, "feature:emscripten_smart_ptr", "1")) {
        Printf(f_wrappers, "\n\t\t.smart_ptr<%sPtr>(\"std::shared_ptr<%s>\")", 
                            className, className);
        
      }
    }

    void printConstructor(Node *n) {

        ParmList *parms = Getattr(n, "parms");

        Printf(f_wrappers, "%s", "\n\t\t.constructor<");

        Parm *p = parms;

        while (p)
        {            
          SwigType *type  = Getattr(p, "type");
          String *typeStr = SwigType_str(type,"");
          Printf (f_wrappers, "%s", typeStr);
          p = nextSibling(p);
          if (p) {            
            Printf (f_wrappers, "%s", ", ");
          }
          else {              
            break;
          }
        }       

        Printf (f_wrappers, "%s", ">()"); 
    }

    void printMemberFunction (Node *n ) {
        
        String   *name      = Getattr(n,"name");        
        String   *symName   = Getattr(n,"sym:name");                  
        String   *className = Getattr(parentNode(n), "name");

        // Skip operator methods 
        if (Strstr(name, "operator "))
        {
          return;
        }

        Printf(f_wrappers, "\n\t\t.function(\"%s\",&%s::%s", symName, className, name); 

        if (checkAttribute (n, "feature:emscripten_allow_raw_pointers", "1")) {
          Printf(f_wrappers, "%s", ", emscripten::allow_raw_pointers()");
        }

        Printf(f_wrappers, "%s", ")");
    }
    
    void printMemberVariable (Node *n) {
        
        String   *symName   = Getattr(n,"sym:name");                          
        String   *name   = Getattr(n,"name");
        String   *className = Getattr(parentNode(n), "name");
        String   *baseNodeType = nodeType(parentNode(n) );

        if (checkAttribute(parentNode(n), "feature:emscripten_value_object", "1") ) {

          Printf(f_wrappers, "\n\t\t.field(\"%s\"", symName);        
        }
        else {
          Printf(f_wrappers, "\n\t\t.property(\"%s\"", symName);  
        }
        

        String   *setter = Getattr(n, "feature:emscripten_setter");
        String   *getter = Getattr(n, "feature:emscripten_getter");

        if (setter || getter) {
          
          if (getter) {

            String   *baseClass = Getattr(n, "feature:emscripten_getter:baseClass");

            if (baseClass) {
              Node *parent = parentNode(n);

              if (parent && checkAttribute(parent, "name", baseClass) && !Strcmp(baseNodeType, "class") ){                
                Printf (f_wrappers, ", &%s", getter);
              }
            }
            else {
              Printf (f_wrappers, ", &%s", getter);
            }

          }

          if (getter) {

            String   *baseClass = Getattr(n, "feature:emscripten_setter:baseClass");

            if (baseClass) {
              Node *parent = parentNode(n);

              if (parent && checkAttribute(parent, "name", baseClass) && !Strcmp(baseNodeType, "class") ){                
                Printf (f_wrappers, ", &%s", setter);
              }
            }
            else {
              Printf (f_wrappers, ", &%s", setter);
            }

          }

        }
        else {
          Printf (f_wrappers, ", &%s::%s", className, name);
        }

        Printf (f_wrappers, "%s", ")");
    }

    void bfsClassHandler(Node* node, std::deque <Node*> & nodes) {

      String *nodeType = nodeType(node);

      if (!Strcmp(nodeType, "enum")) {
        enumDeclaration(node);
        return;
      }

      printBases(node);
      printSharedPtr(node);
      
      typedef std::multimap<std::string, Node*> NodeMap;

      NodeMap members;

      // erase overloaded members

      for (Node * n = firstChild(node); n; n = nextSibling(n)) {

         /* skip non-public members */
        if (!checkAttribute(n, "access", "public")) {
          continue;
        }

        String *symName = Getattr(n, "sym:name");

        if (symName ) {

          String   *nodeType  =  nodeType(n);
                    
          NodeMap::iterator i = members.find(Char(symName) );
          if (i != members.end() && !Strcmp(nodeType, "cdecl")) {

            members.erase(i);
            continue;
          }

          members.insert(std::make_pair<std::string, Node*>(Char(symName), n));
        } 
      }
      
      for (NodeMap::const_iterator i = members.begin(); i != members.end(); ++i) {

        Node *n = i->second;

        String   *nodeType  =  nodeType(n);

        if (!Strcmp(nodeType, "constructor")) {

          if (!checkAttribute(parentNode(n), "feature:emscripten_value_object", "1")) {
            printConstructor(n);
          }

         }
         else if (!Strcmp(nodeType, "cdecl")) {
                    
            if (checkAttribute(n, "kind", "variable")) {
              printMemberVariable(n);
            }
            else if (checkAttribute(n, "kind", "function")) {

              if (!checkAttribute(parentNode(n), "feature:emscripten_value_object", "1")) {
                printMemberFunction(n);
              }
            }
        } else if ( !Strcmp(nodeType, "class") || !Strcmp(nodeType, "struct") ||  !Strcmp(nodeType, "enum")  ){
          nodes.push_front(n);
        }
      }

      Printf(f_wrappers, ";%s\n\n", "");
    }

    virtual int  classHandler(Node *node) { 

      std::deque <Node *> nodes;

      nodes.push_back(node);

      while (!nodes.empty()) {

        Node *node = nodes.back();
        nodes.pop_back();

        bfsClassHandler(node, nodes);
      }

      return SWIG_OK;
      
    }

    virtual int  enumDeclaration(Node *node) { 

      String   *className      = Getattr(node,"name");
      String   *symName   = Getattr(node,"sym:name");                  
      
      Printf(f_wrappers, "\n\tenum_<%s>(\"%s\")", className, symName);

      for (Node * n = firstChild(node); n; n = nextSibling(n)) {
        
        String   *name      = Getattr(n,"name");
        String   *symName   = Getattr(n,"sym:name");                  

        Printf(f_wrappers, "\n\t\t.value(\"%s\", &%s::%s)", symName, className, name);
        
      }

      Printf(f_wrappers, ";%s\n\n");

      return SWIG_OK;
    }

    virtual int  variableWrapper(Node *node) {

      // TODO
      return Language::variableWrapper(node); 
    }

};
      
extern "C" Language *swig_emscripten(void) {
  return new EMSCRIPTEN();
}
