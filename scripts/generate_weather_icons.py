"""Generate native weather masks from the pinned SDK's licensed Lucide SVGs."""
from pathlib import Path
import subprocess
import tempfile
from convert_icon import load_image, image_to_c_array
root=Path(__file__).resolve().parents[1]
icons={'Clear':'sun','Night':'moon','Partly':'cloud-sun','PartlyNight':'cloud-moon','Cloud':'cloud','Fog':'cloud-fog','Rain':'cloud-rain','Snow':'cloud-snow','Storm':'cloud-lightning'}
parts=['#pragma once\n#include <cstdint>\n// Lucide assets; original licence in freeink-sdk/libs/assets/Icons/lucide/LICENSE.\n']
with tempfile.TemporaryDirectory() as temp:
    png=Path(temp)/'weather-menu.png'
    subprocess.run(['rsvg-convert','-w','32','-h','32',str(root/'freeink-sdk/libs/assets/Icons/lucide/icons/cloud-rain.svg'),'-o',str(png)],check=True)
    parts.append(image_to_c_array(load_image(str(png),32,32),'WeatherMenuIcon').replace('#pragma once\n#include <cstdint>\n',''))
    for size in (48, 64):
        for label,source in icons.items():
            png=Path(temp)/(source+str(size)+'.png')
            subprocess.run(['rsvg-convert','-w',str(size),'-h',str(size),str(root/'freeink-sdk/libs/assets/Icons/lucide/icons'/f'{source}.svg'),'-o',str(png)],check=True)
            name='Weather'+label+('Large' if size == 64 else '')+'Icon'
            array=image_to_c_array(load_image(str(png),size,size),name)
            parts.append(array.replace('#pragma once\n#include <cstdint>\n',''))
(root/'src/components/icons/weather.h').write_text('\n'.join(parts))
