import './componentStyles/orbitStyle.css'
import EarthIcon from '../assets/earthEmoji.png'
import SateliteIcon from '../assets/satelite.png'

export default function OrbitDisplay() {
  return (
    <div className="orbit-container-od">
      <div className="earth-orbit-od">
        <div className="earth-core-od">
          <img src={EarthIcon} alt="Earth" className="earth-icon-od" />
        </div>
        <div className="orbit-wrapper-od spin-od">
          <div className="factory-sat-od">
            <img src={SateliteIcon} alt="Satellite" className="satellite-icon-od" />
          </div>
        </div>
      </div>
    </div>
  )
}
