/* ----------------------------------------------------------------------------- 
 * EMSCRIPTEN language module for SWIG.
 * ----------------------------------------------------------------------------- */


#include "swigmod.h"
#include <string>
#include <iostream>
#include <map>
#include <set>
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
       
       String *bindingsStart = NewStringf("%s(%s) { \n", "EMSCRIPTEN_BINDINGS", module);       
       String *bindingsEnd =  NewStringf("%s\n", "} // EMSCRIPTEN_BINDINGS");       
       Dump(bindingsStart, f_begin);
       Dump(f_wrappers, f_begin);
       Dump(bindingsEnd, f_begin);
       Wrapper_pretty_print(f_init, f_begin);
    
       /* Cleanup files */       
       Delete(f_runtime);
       Delete(f_header);
       Delete(f_wrappers);
       Delete(f_init);
       Delete(f_begin);
       Delete(bindingsStart);
       Delete(bindingsEnd);
       
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

      if ( Strstr(symName, "____regmap_") ) {
        String   *regMap      = Getattr(node,"feature:emscripten_register_map");

        Printf(f_wrappers, "\t%s\n", regMap);
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

      if (checkAttribute(n, "feature:emscripten_value_object", "1") && checkBaseForFeature(n, "emscripten_value_object") ) {
        return NewStringf("%s", "value_object");      
      } else {
        return NewStringf("%s", "class_");
      }      
    }

    void printBases(Node *node) {
      
      String   *className = SwigType_str(Getattr(node,"name"), 0);      
      String   *symName = Getattr(node,"sym:name");             

      List *baselist = Getattr(node,"bases");

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
          Printf(f_wrappers, "%s", SwigType_str(Getattr(it.item, "name" ), 0 ) );
          
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

      String   *className = SwigType_str(Getattr(node,"name"), 0);      

      if (checkAttribute(node, "feature:emscripten_smart_ptr", "1")) {
        Printf(f_wrappers, "\n\t\t.smart_ptr<%sPtr>(\"std::shared_ptr<%s>\")", 
                            className, className);
        Printf(f_wrappers, "\n\t\t.smart_ptr<%sConstPtr>(\"std::shared_ptr<const %s>\")", 
                            className, className);
        
      }
    }
    
    void printConstructor(Node *n) {

        ParmList *parms = Getattr(n, "parms");        

        Printf(f_wrappers, "%s", "\n\t\t.constructor<");

        Parm *p = parms;        

        while (p)
        {                    
          String *typeStr = SwigType_str(Getattr(p, "type"), 0);          
          
          Printf (f_wrappers, "%s", typeStr);
          Delete(typeStr);          

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
        
        String   *name      = SwigType_str(Getattr(n,"name"), 0);
        String   *symName   = Getattr(n,"sym:name");                  
        String   *className = SwigType_str(Getattr(parentNode(n),"name"), 0);      

        // Skip operator methods 
        if (Strstr(name, "operator "))
        {
          return;
        }

        // Skip static members

        String *storage = Getattr(n, "storage");
        
        if (Strstr(storage, "static")) {
          return;
        }

        Printf(f_wrappers, "\n\t\t.function(\"%s\",&%s::%s", symName, className, name); 

        if (checkAttribute (n, "feature:emscripten_allow_raw_pointers", "1") && checkBaseForFeature(n, "emscripten_allow_raw_pointers")) {
          Printf(f_wrappers, "%s", ", emscripten::allow_raw_pointers()");
        }

        Printf(f_wrappers, "%s", ")");
    }  

    bool checkBaseForFeature(Node *n, const String_or_char *featureName) {

      String *baseClassAttr = NewStringf("feature:%s:baseClass", featureName);
      String *baseClass = Getattr(n, baseClassAttr);
      Delete(baseClassAttr);

      if (!baseClass) {
        return true;
      }

      Node *parent = parentNode(n);
      if (!parent) {
        return true;
      }

      String   *parentNodeType = nodeType(parent );

      if (Strcmp(parentNodeType, "class") ){
        return true;
      }

      String *name = SwigType_str(Getattr(parent, "name") ,0);
      String *base = SwigType_str(baseClass,0);

      Replace(name, " ", "" ,DOH_REPLACE_ANY);
      Replace(base, " ", "" ,DOH_REPLACE_ANY);


      std::cout << "name: " << Char(name) << "; base: " << Char(SwigType_str(baseClass,0)) << std::endl;
            
      return !Strcmp(name, base);
    } 
    
    void printMemberVariable (Node *n) {
        
        String   *symName   = Getattr(n,"sym:name");                          
        String   *name   = Getattr(n,"name");
        String   *className = SwigType_str(Getattr(parentNode(n),"name"), 0);              

        // Skip static members

        String *storage = Getattr(n, "storage");
        
        if (Strstr(storage, "static")) {
          return;
        }

        if (checkAttribute(parentNode(n), "feature:emscripten_value_object", "1") && 
          checkBaseForFeature(parentNode(n), "emscripten_value_object")) {

          Printf(f_wrappers, "\n\t\t.field(\"%s\"", symName);        
        }
        else {
          Printf(f_wrappers, "\n\t\t.property(\"%s\"", symName);  
        }        

        String   *setter = Getattr(n, "feature:emscripten_setter");
        String   *getter = Getattr(n, "feature:emscripten_getter");

        // TODO: convert base class to SwigType_str

        if (setter || getter) {
          
          if (getter) {

            if (checkBaseForFeature(n, "emscripten_getter")) {            
              Printf (f_wrappers, ", &%s", getter);
            }

          }

          if (getter) {
       
            if (checkBaseForFeature(n, "emscripten_setter")) {                                    
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
      typedef std::set<std::string> MembersToErase;

      NodeMap members;
      MembersToErase membersToErase;


      // erase overloaded members

      for (Node * n = firstChild(node); n; n = nextSibling(n)) {

         /* skip non-public members */
        if (!checkAttribute(n, "access", "public")) {
          continue;
        }

        String *symName = Getattr(n, "sym:name");

        if (symName ) {

          String   *nodeType  =  nodeType(n);
                    
          if (membersToErase.find(Char(symName)) != membersToErase.end()) {
            continue;
          }

          NodeMap::iterator i = members.find(Char(symName) );
          if (i != members.end() && !Strcmp(nodeType, "cdecl")) {

            members.erase(i);
            membersToErase.insert(i->first);

            continue;
          }

          members.insert(std::make_pair<std::string, Node*>(Char(symName), n));
        } 
      }
      
      for (NodeMap::const_iterator i = members.begin(); i != members.end(); ++i) {

        Node *n = i->second;

        String   *nodeType  =  nodeType(n);

        if (!Strcmp(nodeType, "constructor")) {

          if ( (!checkAttribute(parentNode(n), "feature:emscripten_value_object", "1") || !checkBaseForFeature(parentNode(n), "emscripten_value_object" ) ) &&
               (!checkAttribute(parentNode(n), "feature:emscripten_interface", "1") || !checkBaseForFeature(parentNode(n), "emscripten_interface" ) ) )  {
            printConstructor(n);
          }

         }
         else if (!Strcmp(nodeType, "cdecl")) {
                    
            if (checkAttribute(n, "kind", "variable")) {
              printMemberVariable(n);
            }
            else if (checkAttribute(n, "kind", "function")) {

              if (!checkAttribute(parentNode(n), "feature:emscripten_value_object", "1") || !checkBaseForFeature(parentNode(n), "emscripten_value_object" ) ) {
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

      String   *className = SwigType_str(Getattr(node,"name"), 0);      
      String   *symName   = Getattr(node,"sym:name");                  
      
      Printf(f_wrappers, "\n\tenum_<%s>(\"%s\")", className, symName);

      for (Node * n = firstChild(node); n; n = nextSibling(n)) {
        
        String   *name      = Getattr(n,"name");
        String   *symName   = Getattr(n,"sym:name");                  

        Printf(f_wrappers, "\n\t\t.value(\"%s\", %s::%s)", symName, className, name);
        
      }

      Printf(f_wrappers, ";%s\n\n");

      return SWIG_OK;
    }

    virtual int  variableWrapper(Node *node) {

      // TODO
      return SWIG_OK;
      // return Language::variableWrapper(node); 
    }

};
      
extern "C" Language *swig_emscripten(void) {
  return new EMSCRIPTEN();
}
