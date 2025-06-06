import { BrowserRouter as Router, Routes, Route, Link } from 'react-router-dom'
import spaceForgeArt from '/spaceForgeLogoArt.png'
import FactoryOrbitalSiulation from './pages/FactoryOrbitSim'

// Sample pages
const Home = () => (
  <div className="page">
    <img src={spaceForgeArt} className="logo" alt="space forge logo" />
    <h1>Welcome to SpaceForgeOS</h1>
    <p>Simulate the future of orbital semiconductor manufacturing.</p>
  </div>
)

const Factory = () => (
  <div className="page">
    <h2>Orbital Factory View</h2>
    <p>Factory simulation coming soon...</p>
  </div>
)

const Dashboard = () => (
  <div className="page">
    <h2>Production Dashboard</h2>
    <p>Metrics, power, and yields overview.</p>
  </div>
)

function App() {
  return (
    <Router>
      <header style={{ padding: '1rem', borderBottom: '1px solid #333' }}>
        <nav style={{ display: 'flex', gap: '1rem' }}>
          <Link to="/">Home</Link>
          <Link to="/factory">Factory</Link>
          <Link to="/dashboard">Dashboard</Link>
          <Link to="/orbit">Orbit Simulation</Link>
        </nav>
      </header>

      <main style={{ padding: '2rem' }}>
        <Routes>
          <Route path="/" element={<Home />} />
          <Route path="/factory" element={<Factory />} />
          <Route path="/dashboard" element={<Dashboard />} />
          <Route path='/orbit' element={<FactoryOrbitalSiulation />} />
        </Routes>
      </main>
    </Router>
  )
}

export default App
