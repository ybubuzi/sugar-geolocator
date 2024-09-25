import { SugarGeolocator } from '../lib/index.cjs';

const instance = new SugarGeolocator();
instance.addListener('position-change', (coordinate) => {
  console.log(`coordinate: ${coordinate.latitude},${coordinate.longitude}`);
});

instance.addListener('status-change', (state) => {
  console.log(`state: ${state}`);
});
instance.startGeolocator().then(res=>{
  console.log(res)
});
setTimeout(() => {  
  instance.stopGeolocator()
},5000);
