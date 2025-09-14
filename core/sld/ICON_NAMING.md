# Convention de nommage des icônes SVG
Objectif : permettre au frontend de sélectionner dynamiquement le symbole selon le type et l'état.

## Règle générale
`<type>_<state>.svg` en minuscule.

### Exemples
- `cbr_opened.svg`, `cbr_closed.svg`, `cbr_intermediate.svg`
- `ds_opened.svg`, `ds_closed.svg` (disconnector/séparateur)
- `busbar.svg`
- `transformer_2w.svg`, `transformer_3w.svg`

## Localisation
Placez les SVG dans `core/sld/icons/`. Le core ne charge pas ces fichiers ; le frontend les référencera via un alias QML/RC ou un chemin de ressources.
