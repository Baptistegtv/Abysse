# Abysse — jeu homebrew 3DS

Petit poisson-clown qui grandit en mangeant des proies de plus en
plus grosses, à travers 3 zones (Récif → Haute mer → Fosse abyssale).

## Compiler (sur ton PC, pas sur la 3DS)

Il faut **devkitPro** avec le toolchain **devkitARM** + les libs **libctru**,
**citro2d** et **citro3d**.

### 1. Installer devkitPro (une seule fois)

- Va sur https://devkitpro.org/wiki/Getting_Started
- Installe le "devkitPro Pacman" (Windows/Linux/Mac selon ton OS)
- Une fois installé, ouvre un terminal et tape :

```
(sudo) dkp-pacman -S 3ds-dev
```

Ça installe devkitARM + libctru + citro2d + citro3d + les outils (smdhtool, 3dsxtool...).

### 2. Vérifier les variables d'environnement

Normalement l'installeur les configure tout seul, mais si `make` ne trouve pas
devkitARM, vérifie que tu as bien (à adapter selon ton OS) :

```
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=$DEVKITPRO/devkitARM
```

### 3. Compiler le jeu

Depuis le dossier du projet (où il y a le `Makefile`) :

```
make
```

Si tout va bien, ça génère un fichier **`abysse.3dsx`** à la racine du projet.

## Installer sur ta 3DS

1. Branche ta carte SD à ton PC (ou utilise FTP/réseau si tu as ça en place)
2. Copie `abysse.3dsx` dans le dossier `/3ds/` de la carte SD
   (crée le dossier `3ds` à la racine de la SD s'il n'existe pas)
3. Remets la carte SD dans la 3DS
4. Lance le **Homebrew Launcher**, et "Abysse" doit apparaître dans la liste

## Commandes en jeu

- **Stick circulaire ou croix directionnelle** : se déplacer
- **A** (à l'écran titre / game over) : (re)commencer
- **START** : quitter le jeu

## Comment ça marche

- Tu commences petit poisson-clown. Tout ce qui est plus petit que toi
  peut être mangé en le touchant : tu grandis à chaque fois.
- Ce qui est nettement plus gros que toi te repousse et te fait perdre
  un peu de taille si tu le touches — évite-le tant que tu n'es pas
  assez gros.
- Une fois une certaine taille atteinte, tu passes à la zone suivante
  et débloques une nouvelle forme plus impressionnante :
  1. **Poisson-clown** — Le Récif
  2. **Poisson-ballon géant** — La Haute Mer
  3. **Titan des Abysses** — La Fosse Abyssale
- Le score en bas augmente à chaque proie avalée, en fonction de sa taille.

## Pistes pour aller plus loin (si tu veux modifier le code)

Tout est dans `source/main.c`, commenté en français. Quelques idées
faciles à ajouter toi-même :
- Plus de niveaux (ajoute des valeurs dans `SEUIL_NIVEAU` et `NB_NIVEAUX`)
- Des sons (libctru gère l'audio avec `ndsp`)
- De vrais sprites/images au lieu des formes géométriques (citro2d sait
  charger des spritesheets `.t3x`)
- Un menu de sélection de niveau
