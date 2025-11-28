mkfall reference
../build/Release/mr-importer-mesh-example ../bin/models/Models/CarConcept/glTF/CarConcept.gltf 0
cd ..

mkfall lod0
../build/Release/mr-importer-mesh-example ../bin/models/Models/CarConcept/glTF/CarConcept.gltf 1
cd ..

mkfall lod1
../build/Release/mr-importer-mesh-example ../bin/models/Models/CarConcept/glTF/CarConcept.gltf 2
cd ..

mkfall lod2
../build/Release/mr-importer-mesh-example ../bin/models/Models/CarConcept/glTF/CarConcept.gltf 3
cd ..

mkfall lod0diff
parallel flip -r {} -t ../lod0/{/} -b {/.} -c {/.}.csv -hist ::: ../reference/screenshot_*.png
cd ..

mkfall lod1diff
parallel flip -r {} -t ../lod1/{/} -b {/.} -c {/.}.csv -hist ::: ../reference/screenshot_*.png
cd ..

mkfall lod2diff
parallel flip -r {} -t ../lod2/{/} -b {/.} -c {/.}.csv -hist ::: ../reference/screenshot_*.png
cd ..
