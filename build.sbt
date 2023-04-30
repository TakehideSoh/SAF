name := "SAF"

version := "1.3.5"
scalaVersion := "2.12.15"

// javacOptions ++= Seq("-source", "1.8", "-target", "1.8", "-Xlint")
// scalacOptions := Seq("-target:jvm-1.8")

// initialize := {
//   val _ = initialize.value
//   val javaVersion = sys.props("java.specification.version")
//   if (javaVersion != "1.8")
//     sys.error(
//       "Java 1.8 is required for this project. Found " + javaVersion + " instead"
//     )
// }


libraryDependencies += "net.java.dev.jna" % "jna" % "5.13.0"
libraryDependencies += "org.ow2.sat4j" % "org.ow2.sat4j.core" % "2.3.6"
libraryDependencies += "org.scala-lang.modules" %% "scala-parser-combinators" % "2.0.0"
libraryDependencies += "org.colomoto" % "bioLQM" % "0.7.1"

val buildSettings: Seq[Setting[_]] = inThisBuild(
  Seq(
    organization := "fun.scop",
    version := "1.0",
    description := "SAF: SAT-based Attractor Finder"
  )
)

lazy val root = (project in file("."))
  .settings(
    assemblyJarName in assembly := "saf.jar",
    organization := "fun.scop",
    version := "1.0",
    description := "SAF: SAT-based Attractor Finder",
    name := "SAF2023",
    mainClass in assembly := Some("fun.scop.app.an.solver.Saf")
  )

assemblyMergeStrategy := {
  case PathList("META-INF", "MANIFEST.MF") => MergeStrategy.discard
  case _                                   => MergeStrategy.first
}
