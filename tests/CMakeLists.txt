# Les suites de tests du tarot, lancées hors écran par `ctest` depuis la
# racine du build. Chaque exécutable se lie à la bibliothèque d'objets
# pdcartes-tarot-core : les tests voient exactement le code que le jeu
# exécute, et rien n'est compilé deux fois.
#
# Chaque test tourne avec QT_QPA_PLATFORM=offscreen et un HOME qui lui est
# propre, pour que les réglages écrits (profils, expérience des six) ne
# fuient pas d'un test à l'autre ni sur la machine.

set(_model_tests
    appel        # appel du roi à 5 joueurs : camps, restrictions, marque
    roles        # pastilles de rôle des plaques
    seats        # modèle à 3/4/5 joueurs, donnes jusqu'au bout, somme nulle
    bids         # fréquence des paroles des IA à l'enchère
    ally         # l'appelé joue bien en allié du preneur
    hidecall     # l'appel reste caché jusqu'à la carte appelée jouée
    guess        # le cerveau respecte l'appel caché, devinette comprise
    skills       # tempérament et acquises des six, migration des profils
    sheet        # fiche d'un adversaire : édition du tempérament, remises à zéro
    picker       # menu : ordre des sections, tailles des tuiles
)

foreach(_t IN LISTS _model_tests)
    add_executable(test-${_t} test-${_t}.cpp)
    target_link_libraries(test-${_t} PRIVATE pdcartes-tarot-core)
    add_test(NAME ${_t} COMMAND test-${_t})
    set_tests_properties(${_t} PROPERTIES ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;HOME=${CMAKE_CURRENT_BINARY_DIR}/home-${_t}")
endforeach()

# Le pilote UI : une partie lancée à 4 joueurs, une à 5 (avec l'appel cliqué).
# Il lui faut les fontes et icônes embarquées, comme au jeu.
add_executable(test-ui5 test-ui5.cpp)
target_link_libraries(test-ui5 PRIVATE pdcartes-tarot-core)
pdcartes_tarot_embed_resources(test-ui5)
foreach(_seats 4 5)
    add_test(NAME ui5-${_seats} COMMAND test-ui5 ${_seats})
    set_tests_properties(ui5-${_seats} PROPERTIES ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;HOME=${CMAKE_CURRENT_BINARY_DIR}/home-ui5-${_seats}")
endforeach()

# La capture d'écran de la table en pleine enchère, pour voir du yeux.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/shots)
add_executable(test-shot5 test-shot5.cpp)
target_link_libraries(test-shot5 PRIVATE pdcartes-tarot-core)
pdcartes_tarot_embed_resources(test-shot5)
add_test(NAME shot5 COMMAND test-shot5 5 ${CMAKE_CURRENT_BINARY_DIR}/shots/bid-5.png)
set_tests_properties(shot5 PROPERTIES ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;HOME=${CMAKE_CURRENT_BINARY_DIR}/home-shot5")
