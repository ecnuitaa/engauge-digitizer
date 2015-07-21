#include "CallbackGatherXThetaValuesFunctions.h"
#include "CurveConnectAs.h"
#include "Document.h"
#include "EngaugeAssert.h"
#include "ExportFileFunctions.h"
#include "ExportLayoutFunctions.h"
#include "Logger.h"
#include <QTextStream>
#include <QVector>
#include "Spline.h"
#include "SplinePair.h"
#include "Transformation.h"
#include <vector>

using namespace std;

ExportFileFunctions::ExportFileFunctions()
{
}

void ExportFileFunctions::exportAllPerLineXThetaValuesMerged (const DocumentModelExport &modelExport,
                                                              const Document &document,
                                                              const QStringList &curvesIncluded,
                                                              const ExportValuesXOrY &xThetaValues,
                                                              const QString &delimiter,
                                                              const Transformation &transformation,
                                                              QTextStream &str) const
{
  LOG4CPP_INFO_S ((*mainCat)) << "ExportFileFunctions::exportAllPerLineXThetaValuesMerged";

  int curveCount = curvesIncluded.count();
  int xThetaCount = xThetaValues.count();
  QVector<QVector<QString*> > yRadiusValues (curveCount, QVector<QString*> (xThetaCount));
  initializeYRadiusValues (curvesIncluded,
                           xThetaValues,
                           yRadiusValues);
  loadYRadiusValues (modelExport,
                     document,
                     curvesIncluded,
                     transformation,
                     xThetaValues,
                     yRadiusValues);

  outputXThetaYRadiusValues (modelExport,
                             curvesIncluded,
                             xThetaValues,
                             yRadiusValues,
                             delimiter,
                             str);
  destroy2DArray (yRadiusValues);
}

void ExportFileFunctions::exportOnePerLineXThetaValuesMerged (const DocumentModelExport &modelExport,
                                                              const Document &document,
                                                              const QStringList &curvesIncluded,
                                                              const ExportValuesXOrY &xThetaValues,
                                                              const QString &delimiter,
                                                              const Transformation &transformation,
                                                              QTextStream &str) const
{
  LOG4CPP_INFO_S ((*mainCat)) << "ExportFileFunctions::exportOnePerLineXThetaValuesMerged";

  bool isFirst = true;

  QStringList::const_iterator itr;
  for (itr = curvesIncluded.begin(); itr != curvesIncluded.end(); itr++) {

    insertLineSeparator (isFirst,
                         modelExport.header(),
                         str);

    // This curve
    const int CURVE_COUNT = 1;
    QString curveIncluded = *itr;
    QStringList curvesIncluded (curveIncluded);

    int xThetaCount = xThetaValues.count();
    QVector<QVector<QString*> > yRadiusValues (CURVE_COUNT, QVector<QString*> (xThetaCount));
    initializeYRadiusValues (curvesIncluded,
                             xThetaValues,
                             yRadiusValues);
    loadYRadiusValues (modelExport,
                       document,
                       curvesIncluded,
                       transformation,
                       xThetaValues,
                       yRadiusValues);
    outputXThetaYRadiusValues (modelExport,
                               curvesIncluded,
                               xThetaValues,
                               yRadiusValues,
                               delimiter,
                               str);
    destroy2DArray (yRadiusValues);
  }
}

void ExportFileFunctions::exportToFile (const DocumentModelExport &modelExport,
                                        const Document &document,
                                        const Transformation &transformation,
                                        QTextStream &str) const
{
  LOG4CPP_INFO_S ((*mainCat)) << "ExportFileFunctions::exportToFile";

  // Identify curves to be included
  QStringList curvesIncluded = curvesToInclude (modelExport,
                                                document,
                                                document.curvesGraphsNames(),
                                                CONNECT_AS_FUNCTION_SMOOTH,
                                                CONNECT_AS_FUNCTION_STRAIGHT);

  // Delimiter
  const QString delimiter = exportDelimiterToText (modelExport.delimiter());

  // Get x/theta values to be used
  CallbackGatherXThetaValuesFunctions ftor (modelExport,
                                            curvesIncluded,
                                            transformation);
  Functor2wRet<const QString &, const Point &, CallbackSearchReturn> ftorWithCallback = functor_ret (ftor,
                                                                                                     &CallbackGatherXThetaValuesFunctions::callback);
  document.iterateThroughCurvesPointsGraphs(ftorWithCallback);

  ExportValuesXOrY xThetaValuesMerged = ftor.xThetaValues ();

  // Export in one of two layouts
  if (modelExport.layoutFunctions() == EXPORT_LAYOUT_ALL_PER_LINE) {
    exportAllPerLineXThetaValuesMerged (modelExport,
                                        document,
                                        curvesIncluded,
                                        xThetaValuesMerged,
                                        delimiter,
                                        transformation,
                                        str);
  } else {
    exportOnePerLineXThetaValuesMerged (modelExport,
                                        document,
                                        curvesIncluded,
                                        xThetaValuesMerged,
                                        delimiter,
                                        transformation,
                                        str);
  }
}

void ExportFileFunctions::initializeYRadiusValues (const QStringList &curvesIncluded,
                                                   const ExportValuesXOrY &xThetaValuesMerged,
                                                   QVector<QVector<QString*> > &yRadiusValues) const
{
  LOG4CPP_INFO_S ((*mainCat)) << "ExportFileFunctions::initializeYRadiusValues";

  // Initialize every entry with empty string
  int curveCount = curvesIncluded.count();
  int xThetaCount = xThetaValuesMerged.count();
  for (int row = 0; row < xThetaCount; row++) {
    for (int col = 0; col < curveCount; col++) {
      yRadiusValues [col] [row] = new QString;
    }
  }
}

double ExportFileFunctions::linearlyInterpolate (const Points &points,
                                                 double xThetaValue,
                                                 const Transformation &transformation) const
{
  LOG4CPP_INFO_S ((*mainCat)) << "ExportFileFunctions::linearlyInterpolate";

  double yRadius = 0;
  QPointF posGraphBefore; // Not set until ip=1
  bool foundIt = false;
  for (int ip = 0; ip < points.count(); ip++) {

    const Point &point = points.at (ip);
    QPointF posGraph;
    transformation.transformScreenToRawGraph (point.posScreen(),
                                                posGraph);

    if (xThetaValue <= posGraph.x()) {

      foundIt = true;
      if (ip == 0) {

        // Use first point
        yRadius = posGraph.y();

      } else {

        // Between posGraphBefore and posGraph. Note that if posGraph.x()=posGraphBefore.x() then
        // previous iteration of loop would have been used for interpolation, and then the loop was exited
        double s = (xThetaValue - posGraphBefore.x()) / (posGraph.x() - posGraphBefore.x());
        yRadius = (1.0 -s) * posGraphBefore.y() + s * posGraph.y();
      }

      break;
    }

    posGraphBefore = posGraph;
  }

  if (!foundIt) {

    // Use last point
    yRadius = posGraphBefore.y();

  }

  return yRadius;
}

void ExportFileFunctions::loadYRadiusValues (const DocumentModelExport &modelExport,
                                             const Document &document,
                                             const QStringList &curvesIncluded,
                                             const Transformation &transformation,
                                             const ExportValuesXOrY &xThetaValues,
                                             QVector<QVector<QString*> > &yRadiusValues) const
{
  LOG4CPP_INFO_S ((*mainCat)) << "ExportFileFunctions::loadYRadiusValues";

  // Loop through curves
  int curveCount = curvesIncluded.count();
  for (int col = 0; col < curveCount; col++) {

    const QString curveName = curvesIncluded.at (col);

    const Curve *curve = document.curveForCurveName (curveName);
    const Points points = curve->points ();

    if (modelExport.pointsSelectionFunctions() == EXPORT_POINTS_SELECTION_FUNCTIONS_RAW) {

      // No interpolation. Raw points
      loadYRadiusValuesForCurveRaw (points,
                                    xThetaValues,
                                    transformation,
                                    yRadiusValues [col]);
    } else {

      // Interpolation
      if (curve->curveStyle().lineStyle().curveConnectAs() == CONNECT_AS_FUNCTION_SMOOTH) {

        loadYRadiusValuesForCurveInterpolatedSmooth (points,
                                                     xThetaValues,
                                                     transformation,
                                                     yRadiusValues [col]);

      } else {

        loadYRadiusValuesForCurveInterpolatedStraight (points,
                                                       xThetaValues,
                                                       transformation,
                                                       yRadiusValues [col]);
      }
    }
  }
}

void ExportFileFunctions::loadYRadiusValuesForCurveInterpolatedSmooth (const Points &points,
                                                                       const ExportValuesXOrY &xThetaValues,
                                                                       const Transformation &transformation,
                                                                       QVector<QString*> &yRadiusValues) const
{
  LOG4CPP_INFO_S ((*mainCat)) << "ExportFileFunctions::loadYRadiusValuesForCurveInterpolatedSmooth";

  // Iteration accuracy versus number of iterations 8->256, 10->1024, 12->4096. Single pixel accuracy out of
  // typical image size of 1024x1024 means around 10 iterations gives decent accuracy
  const int MAX_ITERATIONS = 12;

  // Load spline pairs
  vector<double> t;
  vector<SplinePair> xy;

  Points::const_iterator itrP;
  for (itrP = points.begin(); itrP != points.end(); itrP++) {
    const Point &point = *itrP;
    QPointF posScreen = point.posScreen();
    QPointF posGraph;
    transformation.transformScreenToRawGraph (posScreen,
                                              posGraph);

    t.push_back (point.ordinal ());
    xy.push_back (SplinePair (posGraph.x(),
                              posGraph.y()));
  }

  // Fit a spline
  Spline spline (t,
                 xy);

  // Get value at desired points
  for (int row = 0; row < xThetaValues.count(); row++) {

    double xTheta = xThetaValues.at (row);
    SplinePair splinePairFound = spline.findSplinePairForFunctionX (xTheta,
                                                                    MAX_ITERATIONS);
    double yRadius = splinePairFound.y ();

    // Save value for this row
    *(yRadiusValues [row]) = QString::number (yRadius);
  }
}

void ExportFileFunctions::loadYRadiusValuesForCurveInterpolatedStraight (const Points &points,
                                                                         const ExportValuesXOrY &xThetaValues,
                                                                         const Transformation &transformation,
                                                                         QVector<QString*> &yRadiusValues) const
{
  LOG4CPP_INFO_S ((*mainCat)) << "ExportFileFunctions::loadYRadiusValuesForCurveInterpolatedStraight";

  // Get value at desired points
  for (int row = 0; row < xThetaValues.count(); row++) {

    double xThetaValue = xThetaValues.at (row);

    double yRadius = linearlyInterpolate (points,
                                          xThetaValue,
                                          transformation);



    // Save value for this row
    *(yRadiusValues [row]) = QString::number (yRadius);
  }
}

void ExportFileFunctions::loadYRadiusValuesForCurveRaw (const Points &points,
                                                        const ExportValuesXOrY &xThetaValues,
                                                        const Transformation &transformation,
                                                        QVector<QString*> &yRadiusValues) const
{
  LOG4CPP_INFO_S ((*mainCat)) << "ExportFileFunctions::loadYRadiusValuesForCurveRaw";

  // Since the curve points may be a subset of xThetaValues (in which case the non-applicable xThetaValues will have
  // blanks for the yRadiusValues), we iterate over the smaller set
  for (int pt = 0; pt < points.count(); pt++) {

    const Point &point = points.at (pt);

    QPointF posGraph;
    transformation.transformScreenToRawGraph (point.posScreen(),
                                              posGraph);

    // Find the closest point in xThetaValues. This is probably an N-squared algorithm, which is less than optimial,
    // but the delay should be insignificant with normal-sized export files
    double closestSeparation = 0.0;
    int rowClosest = 0;
    for (int row = 0; row < xThetaValues.count(); row++) {

      double xThetaValue = xThetaValues.at (row);

      double separation = qAbs (posGraph.x() - xThetaValue);

      if ((row == 0) ||
          (separation < closestSeparation)) {

        closestSeparation = separation;
        rowClosest = row;

      }
    }

    // Save value for this row
    *(yRadiusValues [rowClosest]) = QString::number (posGraph.y());
  }
}

void ExportFileFunctions::outputXThetaYRadiusValues (const DocumentModelExport &modelExport,
                                                     const QStringList &curvesIncluded,
                                                     const ExportValuesXOrY &xThetaValuesMerged,
                                                     QVector<QVector<QString*> > &yRadiusValues,
                                                     const QString &delimiter,
                                                     QTextStream &str) const
{
  LOG4CPP_INFO_S ((*mainCat)) << "ExportFileFunctions::outputXThetaYRadiusValues";

  // Header
  str << modelExport.xLabel();
  QStringList::const_iterator itrHeader;
  for (itrHeader = curvesIncluded.begin(); itrHeader != curvesIncluded.end(); itrHeader++) {
    QString curveName = *itrHeader;
    str << delimiter << curveName;
  }
  str << "\n";

  for (int row = 0; row < xThetaValuesMerged.count(); row++) {

    double xTheta = xThetaValuesMerged.at (row);

    str << xTheta;

    for (int col = 0; col < yRadiusValues.count(); col++) {

      str << delimiter << *(yRadiusValues [col] [row]);
    }

    str << "\n";
  }
}
