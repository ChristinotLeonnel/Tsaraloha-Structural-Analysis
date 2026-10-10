"""Écrit le fichier .def (exports) d'une DLL Windows x64, pour générer une bibliothèque d'importation MSVC
(lib /def:…) permettant le chargement différé (/DELAYLOAD). Utilisé pour le SDK Gmsh (compilé avec MinGW).

    python dll_to_def.py <fichier.dll> <sortie.def> [préfixe]

Avec un préfixe (ex. « gmsh »), seuls les symboles qui commencent par lui sont retenus (API C de Gmsh :
la DLL exporte aussi tous les symboles internes de ses dépendances statiques).
"""
import os
import struct
import sys


def exports(path):
    data = open(path, 'rb').read()
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    if data[pe:pe + 4] != b'PE\0\0':
        raise ValueError('pas un fichier PE')
    nsec = struct.unpack_from('<H', data, pe + 6)[0]
    optsize = struct.unpack_from('<H', data, pe + 20)[0]
    opt = pe + 24
    magic = struct.unpack_from('<H', data, opt)[0]
    ddir = opt + (112 if magic == 0x20B else 96)
    exp_rva, exp_size = struct.unpack_from('<II', data, ddir)
    sections = []
    sec = opt + optsize
    for i in range(nsec):
        vsize, vaddr, rsize, raddr = struct.unpack_from('<IIII', data, sec + 40 * i + 8)
        sections.append((vaddr, max(vsize, rsize), raddr))

    def off(rva):
        for vaddr, size, raddr in sections:
            if vaddr <= rva < vaddr + size:
                return rva - vaddr + raddr
        raise ValueError('RVA hors sections')

    if not exp_rva:
        return []
    e = off(exp_rva)
    nnames = struct.unpack_from('<I', data, e + 24)[0]
    names_rva = struct.unpack_from('<I', data, e + 32)[0]
    names = []
    for i in range(nnames):
        nrva = struct.unpack_from('<I', data, off(names_rva) + 4 * i)[0]
        o = off(nrva)
        names.append(data[o:data.index(b'\0', o)].decode('ascii'))
    return names


def main():
    dll, out = sys.argv[1], sys.argv[2]
    names = exports(dll)
    if len(sys.argv) > 3:
        names = [n for n in names if n.startswith(sys.argv[3])]
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, 'w', encoding='ascii') as f:
        f.write('LIBRARY "%s"\nEXPORTS\n' % os.path.basename(dll))
        for n in names:
            f.write('    %s\n' % n)
    print('%d exports' % len(names))


if __name__ == '__main__':
    main()
