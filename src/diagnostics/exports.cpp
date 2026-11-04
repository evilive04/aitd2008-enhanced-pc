// Preserve x86 stack arguments and return directly from the system implementation.
extern "C" void* exports[23];
extern "C" void __cdecl EnsureExports();
extern "C" __declspec(naked) void Forward0() {
    __asm {
        pushfd
        pushad
        call EnsureExports
        popad
        popfd
        jmp dword ptr [exports + 0]
    }
}
extern "C" __declspec(naked) void Forward1() {
    __asm {
        pushfd
        pushad
        call EnsureExports
        popad
        popfd
        jmp dword ptr [exports + 4]
    }
}
extern "C" __declspec(naked) void Forward2() {
    __asm {
        pushfd
        pushad
        call EnsureExports
        popad
        popfd
        jmp dword ptr [exports + 8]
    }
}
extern "C" __declspec(naked) void Forward3() {
    __asm {
        pushfd
        pushad
        call EnsureExports
        popad
        popfd
        jmp dword ptr [exports + 12]
    }
}
extern "C" __declspec(naked) void Forward4() {
    __asm {
        pushfd
        pushad
        call EnsureExports
        popad
        popfd
        jmp dword ptr [exports + 16]
    }
}
extern "C" __declspec(naked) void Forward5() {
    __asm {
        pushfd
        pushad
        call EnsureExports
        popad
        popfd
        jmp dword ptr [exports + 20]
    }
}
extern "C" __declspec(naked) void Forward6() {
    __asm {
        pushfd
        pushad
        call EnsureExports
        popad
        popfd
        jmp dword ptr [exports + 24]
    }
}
extern "C" __declspec(naked) void Forward7() {
    __asm {
        pushfd
        pushad
        call EnsureExports
        popad
        popfd
        jmp dword ptr [exports + 28]
    }
}
extern "C" __declspec(naked) void Forward8() {
    __asm {
        pushfd
        pushad
        call EnsureExports
        popad
        popfd
        jmp dword ptr [exports + 32]
    }
}
extern "C" __declspec(naked) void Forward9() {
    __asm {
        pushfd
        pushad
        call EnsureExports
        popad
        popfd
        jmp dword ptr [exports + 36]
    }
}
extern "C" __declspec(naked) void Forward10() {
    __asm {
        pushfd
        pushad
        call EnsureExports
        popad
        popfd
        jmp dword ptr [exports + 40]
    }
}
extern "C" __declspec(naked) void Forward11() {
    __asm {
        pushfd
        pushad
        call EnsureExports
        popad
        popfd
        jmp dword ptr [exports + 44]
    }
}
extern "C" __declspec(naked) void Forward12() {
    __asm {
        pushfd
        pushad
        call EnsureExports
        popad
        popfd
        jmp dword ptr [exports + 48]
    }
}
extern "C" __declspec(naked) void Forward13() {
    __asm {
        pushfd
        pushad
        call EnsureExports
        popad
        popfd
        jmp dword ptr [exports + 52]
    }
}
extern "C" __declspec(naked) void Forward14() {
    __asm {
        pushfd
        pushad
        call EnsureExports
        popad
        popfd
        jmp dword ptr [exports + 56]
    }
}
extern "C" __declspec(naked) void Forward15() {
    __asm {
        pushfd
        pushad
        call EnsureExports
        popad
        popfd
        jmp dword ptr [exports + 60]
    }
}
extern "C" __declspec(naked) void Forward16() {
    __asm {
        pushfd
        pushad
        call EnsureExports
        popad
        popfd
        jmp dword ptr [exports + 64]
    }
}
extern "C" __declspec(naked) void Forward17() {
    __asm {
        pushfd
        pushad
        call EnsureExports
        popad
        popfd
        jmp dword ptr [exports + 68]
    }
}
extern "C" __declspec(naked) void Forward18() {
    __asm {
        pushfd
        pushad
        call EnsureExports
        popad
        popfd
        jmp dword ptr [exports + 72]
    }
}
extern "C" __declspec(naked) void Forward19() {
    __asm {
        pushfd
        pushad
        call EnsureExports
        popad
        popfd
        jmp dword ptr [exports + 76]
    }
}
extern "C" __declspec(naked) void Forward20() {
    __asm {
        pushfd
        pushad
        call EnsureExports
        popad
        popfd
        jmp dword ptr [exports + 80]
    }
}
extern "C" __declspec(naked) void Forward22() {
    __asm {
        pushfd
        pushad
        call EnsureExports
        popad
        popfd
        jmp dword ptr [exports + 88]
    }
}
