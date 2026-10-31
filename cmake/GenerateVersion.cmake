# Ce que le jeu dit de lui-même : la version qu'on lui a donnée, et l'instant
# où ce build-ci a été fait.
#
# La version vient du fichier VERSION, avancé au moment de publier ; le
# tampon est pris ici à chaque build, de sorte que deux builds d'une même
# version restent distingués -- ce qu'une version seule ne peut pas faire
# pour une chose qu'on reconstruit plutôt qu'on ne publie.
#
# Écrit sous deux formes : un fichier source que le programme emporte, et une
# ligne nue que l'empaquetage lit, cpack tournant longtemps après que le
# compilateur a fini.
#
# Avec RELEASE, pas d'empreinte du tout : une version se nomme par son numéro
# et rien d'autre, et deux paquets d'une même version n'ont pas à se
# distinguer -- il n'y en a qu'un. C'est la cible release qui le demande, et
# elle seule ; voir CMakeLists.txt.

# L'empaquetage reconstruit le programme depuis cpack, après que cpack a lu
# la version dont il nomme le paquet. Retamponner là mettrait une version sur
# le paquet et une plus tardive dans le binaire qui est dedans : le tampon
# est donc laissé tranquille pendant l'empaquetage.
if(DEFINED ENV{PDCARTES_TAROT_KEEP_VERSION} AND EXISTS "${OUT_TXT}")
    return()
endif()

if(RELEASE)
    set(BUILD_VERSION "")
else()
    string(TIMESTAMP BUILD_VERSION "%y%m%d%H%M%S")
endif()

file(WRITE "${OUT_CPP}"
"// Écrit à chaque build par cmake/GenerateVersion.cmake. Ne pas modifier, et
// ne pas versionner : c'est ce build-ci, et non cette source-ci.
//
// extern, parce qu'une constante de portée fichier appartiendrait sinon à ce
// fichier seul, alors que le programme doit les lire d'ailleurs.
extern const char *const kPdcartesTarotVersion;
const char *const kPdcartesTarotVersion = \"${VERSION}\";
extern const char *const kPdcartesTarotBuildVersion;
const char *const kPdcartesTarotBuildVersion = \"${BUILD_VERSION}\";
")

file(WRITE "${OUT_TXT}" "${BUILD_VERSION}")
