# Cas de test

- [Conflits](#Conflits)
    - [Conflit avec un placeholder déshydraté](#conflit-avec-un-placeholder-deshydrate)
    - [Types de conflits](#types-de-conflits)
        - [Create-Create](#create-create)
        - [Edit-Edit](#edit-edit)
        - [Move-Create](#move-create)
        - [Edit-Delete](#edit-delete)
        - [Move-Delete](#move-delete)
        - [Move-ParentDelete](#move-parentdelete)
        - [Create-ParentDelete](#create-parentdelete)
        - [Move-Move (Source)](#move-move-source)
        - [Move-Move (Dest)](#move-move-dest)
        - [Move-Move (Cycle)](#move-move-cycle)

## Conflits

### Conflit avec un placeholder déshydraté
_Situation initiale:_
- Créer un dossier `test_conflit`.
- Synchroniser un fichier `A.txt` en mode LiteSync.
- Vérifier que le fichier `A.txt` est bien déshydraté.

_Etapes:_
- Mettre la synchronisation en pause.
- En local, déplacer `A.txt` vers `test_conflit/A.txt`.
- En remote, créer un fichier `test_conflit/A.txt`.
- Relancer la synchronisation.

_Résultat attendu:_
L'opération local est annulé (fichier supprimé puis re-créer à son emplacement d'origine). Les 2 fichiers doivent être synchrinisés:
```
.
├── test_conflits
│   └── A.txt
└── A.txt
```

### Types de conflits
Les tests suivants sont à faire avec la LiteSync désactivée ou avec dans fichiers hydratés.

#### Create-Create
_Situation initiale:_

_Etapes:_
- Mettre la synchronisation en pause.
- En local, créer le fichier `A.txt`.
- En remote, créer le fichier `A.txt`.
- Relancer la synchronisation.

_Résultat attendu:_
- Le fichier local est renommé avec un suffix `_conflict_`. Il n'est pas synchronisé.
- Le ficher est ensuite re-créer en local.

#### Edit-Edit
_Situation initiale:_
- Créer un fichier `A.txt`.
- Attendre qu'il soit synchronisé.

_Etapes:_
- Mettre la synchronisation en pause.
- En local, éditer un fichier `A.txt`.
- En remote, éditer un fichier `A.txt`.
- Relancer la synchronisation.

_Résultat attendu:_
- Le fichier local est renommé avec un suffix `_conflict_`. Il n'est pas synchronisé.
- Le ficher est ensuite re-créer en local.

#### Move-Create

##### Operation Move sur le fichier local

_Situation initiale:_
- Créer un fichier `A.txt`.
- Attendre qu'il soit synchronisé.

_Etapes:_
- Mettre la synchronisation en pause.
- En local, renommer `A.txt` en `B.txt`.
- En remote, créer un fichier `B.txt`.
- Relancer la synchronisation.

_Résultat attendu_:
L'opération local est annulé (fichier supprimé puis re-créer à son emplacement d'origine). Les 2 fichiers doivent être synchrinisés

##### Operation Move sur le fichier remote

_Situation initiale:_
- Créer un fichier `A.txt`.
- Attendre qu'il soit synchronisé.

_Etapes:_
- Mettre la synchronisation en pause.
- En remote, renommer `A.txt` en `B.txt`.
- En local, créer un fichier `B.txt`.
- Relancer la synchronisation.

_Résultat attendu:_
- Le fichier local est renommé avec un suffix `_conflict_`. Il n'est pas synchronisé.
- Le ficher remote est ensuite créer en local.

#### Edit-Delete
_Situation initiale:_
- Créer un fichier `A.txt`.
- Attendre qu'il soit synchronisé.

_Etapes:_
- Mettre la synchronisation en pause.
- En local, .
- En remote, .
- Relancer la synchronisation.

_Résultat attendu:_

#### Move-Delete
_Situation initiale:_

_Etapes:_
- Mettre la synchronisation en pause.
- En local, .
- En remote, .
- Relancer la synchronisation.

_Résultat attendu:_

#### Move-ParentDelete
_Situation initiale:_

_Etapes:_
- Mettre la synchronisation en pause.
- En local, .
- En remote, .
- Relancer la synchronisation.

_Résultat attendu:_

#### Create-ParentDelete
_Situation initiale:_

_Etapes:_
- Mettre la synchronisation en pause.
- En local, .
- En remote, .
- Relancer la synchronisation.

_Résultat attendu:_

#### Move-Move (Source)
_Situation initiale:_

_Etapes:_
- Mettre la synchronisation en pause.
- En local, .
- En remote, .
- Relancer la synchronisation.

_Résultat attendu:_

#### Move-Move (Dest)
_Situation initiale:_

_Etapes:_
- Mettre la synchronisation en pause.
- En local, .
- En remote, .
- Relancer la synchronisation.

_Résultat attendu:_

#### Move-Move (Cycle)
_Situation initiale:_

_Etapes:_
- Mettre la synchronisation en pause.
- En local, .
- En remote, .
- Relancer la synchronisation.

_Résultat attendu:_







### Template

_Situation initiale:_

_Etapes:_
- Mettre la synchronisation en pause.
- 
- Relancer la synchronisation.

_Résultat attendu:_
