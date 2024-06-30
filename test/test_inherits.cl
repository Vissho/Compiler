class Main inherits Other { 
     
} ;

class A inherits D{ 
    main() : Object { 
        out_string("Hello, world.\n") 
    } ; 
} ; 

class B inherits A { 
    main() : Object { 
        out_string("Hello, world.\n") 
    } ; 
} ; 

class C inherits B { 
    main() : Object { 
        out_string("Hello, world.\n") 
    } ; 
} ; 

class D inherits B { 
    main() : Object { 
        out_string("Hello, world.\n") 
    } ; 
} ; 
