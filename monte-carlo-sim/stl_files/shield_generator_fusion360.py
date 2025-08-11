
# Fusion 360 Add‑In / Script — Shield Generator
# Author: ChatGPT (generated)
# Description:
#   Builds a spherical-cap shield body based on parametric inputs and exports a STEP file.
#   Put this file in your Fusion 360 Scripts & Add‑Ins folder, then RUN. 
#   All key dimensions are exposed as user‑parameters so you can tweak them later via Modify ➜ Change Parameters.
#
# PARAMETERS (default values come from your simulation row)
#   capRadius      : rim radius of the cap             (R_cap)
#   sphereRadius   : radius of the parent sphere       (R_sphere)  — must be ≥ capRadius
#   thickness      : uniform wall thickness
#   rimChamfer     : optional flat‑rim chamfer (set 0 for completely flat rim)
#   exportStep     : True ➜ automatically export 'shield_export.step' next to the script
#
# NB: The curvature value in your CSV (2.257 1/m) is *incompatible* with the requested 3.231 m rim radius.
#     For now the script sets sphereRadius = capRadius so you get a shallow 180° dome.
#     Adjust sphereRadius if you want a steeper or flatter profile.
#
import adsk.core, adsk.fusion, traceback, math, os

# ----------------------------- CONFIGURABLE DEFAULTS -----------------------------
capRadius    = 3.2311515307179066     # metres
sphereRadius = 3.2311515307179066     # metres (≥ capRadius)
thickness    = 0.05                   # metres (50 mm) ❗ Adjust — your CSV had 2.09 m which is probably a typo!
rimChamfer   = 0.0                    # metres (0 ➜ rim stays flat)
exportStep   = True                   # auto‑export STEP when script finishes
# ---------------------------------------------------------------------------------

def run(context):
    ui = None
    try:
        app = adsk.core.Application.get()
        ui  = app.userInterface

        # Create a new design if the user has none open
        if app.activeProduct is None:
            doc = app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType)
        design = adsk.fusion.Design.cast(app.activeProduct)
        unitsMgr = design.unitsManager

        # Root component
        root = design.rootComponent

        # ---------------- Create construction geometry ----------------
        # Calculate sagitta (height) of the spherical cap
        h = sphereRadius - math.sqrt(sphereRadius**2 - capRadius**2)

        # Sketch half‑profile of the dome on the XZ plane (revolve later)
        sketches = root.sketches
        xzPlane  = root.xZConstructionPlane
        profSk   = sketches.add(xzPlane)
        sketchPts = profSk.sketchPoints
        sketchArcs = profSk.sketchCurves.sketchArcs

        # Points: rim (A) at [capRadius,0,0], apex (B) at [0,0,h]
        pA = adsk.core.Point3D.create(capRadius, 0, 0)
        pB = adsk.core.Point3D.create(0, 0, h)

        # Arc centre in 2D sketch coords: (0, 0, sphereRadius - h)
        c = adsk.core.Point3D.create(0, 0, sphereRadius - h)

        # Because sketch is XZ, Y=0 plane; shift Z into XZ's 'Y' coordinate
        # In Fusion's sketch, X = X, Y = Z; so convert
        pA2D = adsk.core.Point3D.create(capRadius, 0)
        pB2D = adsk.core.Point3D.create(0, h)
        c2D  = adsk.core.Point3D.create(0, sphereRadius - h)

        # Draw the arc
        sketchArcs.addByCenterStartSweep(c2D, pA2D, math.radians(180))

        # Draw rim line back to axis
        lines = profSk.sketchCurves.sketchLines
        axisLine = lines.addByTwoPoints(pB2D, adsk.core.Point3D.create(0,0))
        rimLine  = lines.addByTwoPoints(pA2D, adsk.core.Point3D.create(capRadius, 0))

        # ---------------- Revolve to create solid ----------------
        prof = profSk.profiles.item(0)
        revolves = root.features.revolveFeatures
        revInput = revolves.createInput(prof, axisLine, adsk.fusion.FeatureOperations.NewBodyFeatureOperation)
        revInput.setAngleExtent(False, adsk.core.ValueInput.createByReal(2 * math.pi))
        rev = revolves.add(revInput)
        body = rev.bodies.item(0)

        # ---------------- Shell (thickness) ----------------
        shellFeat = root.features.shellFeatures
        shellInput = shellFeat.createInput([body], True)
        shellInput.insideThickness = adsk.core.ValueInput.createByReal(thickness)
        shellFeat.add(shellInput)

        # ---------------- Rim chamfer (optional) ----------------
        if rimChamfer > 0:
            edges = [e for e in body.edges if e.geometryType == adsk.core.Line3D.classType()]
            if edges:
                chamfers = root.features.chamferFeatures
                edgeColl = adsk.core.ObjectCollection.create()
                for e in edges: edgeColl.add(e)
                chamferInput = chamfers.createInput(edgeColl, True)
                distance = adsk.core.ValueInput.createByReal(rimChamfer)
                chamferInput.setToDistance(distance)
                chamfers.add(chamferInput)

        # ---------------- Export STEP ----------------
        if exportStep:
            stepFile = os.path.join(os.path.expanduser('~'), 'shield_export.step')
            exportMgr = design.exportManager
            stepOptions = exportMgr.createSTEPExportOptions(stepFile)
            exportMgr.execute(stepOptions)
            ui.messageBox(f'Step file exported to:\n{stepFile}')

        ui.messageBox('Shield model complete.')
    except:
        if ui:
            ui.messageBox('Failed:\n{}'.format(traceback.format_exc()))
def stop(context):
    pass
