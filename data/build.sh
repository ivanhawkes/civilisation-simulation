#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

# Merge the given yaml files into a single game-ready document:
#   - explode(.) resolves every anchor/alias (the relative links between the
#     files) so the result is easy to read and process.
#   - the lists under each game key are concatenated, so a key such as items:
#     that is spread over several files appears exactly once.
#   - the *-base keys are not copied: their only purpose is to define the
#     anchors used by the relative links, which explode(.) has inlined.
#   - Note: yq's * operator replaces a repeated key and = appends a duplicate
#     one, so += is used to concatenate the lists under a single key.
merge_documents() {
  for file in "$@"; do
    cat "$file"
    printf '\n'
  done \
    | yq ea \
      --yaml-fix-merge-anchor-to-spec=true \
      '
        explode(.)
        | . as $doc ireduce ({};
            .ages          += ($doc.ages // [])
          | .categories    += ($doc.categories // [])
          | .skills        += ($doc.skills // [])
          | .items         += ($doc.items // []))
      '
}

# The tech-tree without an item list: the ages, the categories and every item
# definition with its anchors. The item lists in 60-items-*.yaml pick from them.
TECH_TREE=(
  base/ages.yaml
  trade-items/categories.yaml
  trade-items/chemicals.yaml
  trade-items/metals.yaml
  trade-items/minerals.yaml
  trade-items/misc.yaml
  trade-items/seeds.yaml
  skill-tree/skills.yaml
)

# Merge the whole tech-tree, including every item in the game.
merge_documents "${TECH_TREE[@]}" game-mode/full.yaml > core-config/full.yaml

# Merge the subset of items used by the simple version of the game, for quick
# matches and testing.
merge_documents "${TECH_TREE[@]}" game-mode/simple.yaml > core-config/simple.yaml

# Process the ages into a form useful for the game, exploding all the anchors
# and dropping the base list that only exists to define them.
yq -n \
  --yaml-fix-merge-anchor-to-spec=true \
  'load("base/ages.yaml") | explode(.) | del(.ages-base)' \
  > core-config/ages.yaml

# Process the categories into a form useful for the game, exploding all the
# anchors and dropping the base list that only exists to define them.
yq -n \
  --yaml-fix-merge-anchor-to-spec=true \
  'load("trade-items/categories.yaml") | explode(.) | del(.categories-base)' \
  > core-config/categories.yaml
