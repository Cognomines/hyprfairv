# hyprfairv

A Hyprland tiling layout plugin that implements a fair vertical tiling algorithm.

## Installation

### Using hyprpm (Recommended)

```bash
hyprpm add https://github.com/cognomines/hyprfairv
hyprpm update
hyprpm enable hyprfairv
```

### Manual Build

```bash
make -j$(nproc)
```

Or load plugins from the config
```
plugin = /path/to/whereever/fairvLayoutPlugin.so
```

### Loading plugin

```bash
hyprctl plugin load ./fairvLayoutPlugin.so
```

### Unloading plugin

```bash
cd /path/to/hyprfairv
hyprctl plugin unload $(realpath *.so)
```
or 
```
hyprctl plugin unload /path/to/where/hyprfairv/fairvLayoutPlugin.so
```

## Plugin Configuration settings
Looking inside the main.cpp you can see settings for the plugin.

```
plugin:fairv:layout:no_gaps_when_only = 0     # 1 = remove gaps when only one window
plugin:fairv:layout:special_scale_factor = 0.8  # Scale factor for special workspaces
plugin:fairv:layout:inherit_fullscreen = 1      # 1 = inherit fullscreen behavior
```

## Usage

The fairv layout automatically tiles windows vertically in a fair distribution. When you open multiple windows, they are arranged side-by-side with equal widths.

### Change to 'fairv' layout

```bash
hyprctl keyword "workspace <workspace_id>, layout:fairv"
```
### cycle-next-layout.sh

As of hyprland 0.54.0 we add fairv to its layout

```bash
#!/bin/bash

layouts=("dwindle" "master" "monocle" "scrolling" "fairv")

json=$(hyprctl activeworkspace -j)
workspace_id=$(echo "$json" | jq -r '.id')
current=$(echo "$json" | jq -r '.tiledLayout')

for i in "${!layouts[@]}"; do
    if [[ "${layouts[i]}" == "$current" ]]; then
        next_index=$(( (i + 1) % ${#layouts[@]} ))
        break
    fi
done

if [[ -z "$next_index" ]]; then
    next_index=0
fi

nextlayout="${layouts[$next_index]}"
hyprctl keyword "workspace $workspace_id, layout:$nextlayout"
notify-send -u low --replace-id=1 "Layout" "$nextlayout"
```

### cycle-prev-layout.sh

```bash
#!/bin/bash

layouts=("dwindle" "master" "monocle" "scrolling" "fairv")

json=$(hyprctl activeworkspace -j)
workspace_id=$(echo "$json" | jq -r '.id')
current=$(echo "$json" | jq -r '.tiledLayout')

for i in "${!layouts[@]}"; do
    if [[ "${layouts[i]}" == "$current" ]]; then
        prev_index=$(( (i - 1 + ${#layouts[@]}) % ${#layouts[@]} ))
        break
    fi
done

if [[ -z "$prev_index" ]]; then
    prev_index=0
fi

prevlayout="${layouts[$prev_index]}"
hyprctl keyword "workspace $workspace_id, layout:$prevlayout"
notify-send -u low --replace-id=1 "Layout" "$prevlayout"
```

### Keybinds

Bind whatever keybindings
Standard Hyprland keybinds for window management work with this layout:

## Let me know if there is a miscognomen 

Let me know if I need to change the name and refactor as 'fairh'.

## License

BSD 3-Clause License

