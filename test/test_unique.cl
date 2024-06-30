class B { 
    main() : Int { 
        10 
    } ; 
} ;

class B { 
    main() : Object { 
        out_string("Hello, world.\n") 
    } ; 
} ; 

class C { 
    xcar : Int;
    xcar : Bool;

    isNil() : Bool { false };
    isNil() : Int { 10 };
} ;

class D { 
    insert(i : Int, i : Bool) : Int {
		if i < 10 then
			0
		else
			1
		fi
	};
} ;