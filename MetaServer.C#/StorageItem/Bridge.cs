using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Xml.Linq;

namespace Consensus
{

    namespace Track
    {
        public struct Control
        {
            public Vec3     Position,
                            Direction,
                            Up;				//!< Up axis. Must be normalized and orthogonal to @a direction
            public Vec2     Scale;
            public float    Angle0,
                            Angle1,
                            Texcoord0,
                            Texcoord1;


            public static Control operator +(Control a, Control b)
            {
                return new Control()
                {
                    Position = a.Position + b.Position,
                    Direction = a.Direction + b.Direction,
                    Up = a.Up + b.Up,
                    Scale = a.Scale + b.Scale,
                    Angle0 = a.Angle0 + b.Angle0,
                    Angle1 = a.Angle1 + b.Angle1,
                    Texcoord0 = a.Texcoord0 + b.Texcoord0,
                    Texcoord1 = a.Texcoord1 + b.Texcoord1
                };
            }
            public static Control operator -(Control a, Control b)
            {
                return new Control()
                {
                    Position = a.Position - b.Position,
                    Direction = a.Direction - b.Direction,
                    Up = a.Up - b.Up,
                    Scale = a.Scale - b.Scale,
                    Angle0 = a.Angle0 - b.Angle0,
                    Angle1 = a.Angle1 - b.Angle1,
                    Texcoord0 = a.Texcoord0 - b.Texcoord0,
                    Texcoord1 = a.Texcoord1 - b.Texcoord1
                };
            }
            public static Control operator *(Control a, float f)
            {
                return new Control()
                {
                    Position = a.Position * f,
                    Direction = a.Direction * f,
                    Up = a.Up * f,
                    Scale = a.Scale * f,
                    Angle0 = a.Angle0 * f,
                    Angle1 = a.Angle1 * f,
                    Texcoord0 = a.Texcoord0 * f,
                    Texcoord1 = a.Texcoord1 * f
                };
            }
            public static Control operator /(Control a, float f)
            {
                return a * (1.0f / f);
            }
        }

        public struct InterpolatedSlice
        {
            public Control control;
            public Vec3 right;
            public float t,
                            texcoord_y;

            public InterpolatedSlice(Control slice, float t_)
            {
                control = slice;
                t = t_;
                right = Vec.Cross(control.Direction, control.Up);
                texcoord_y = 0;
            }
        }

        public struct Connector
        {
            public Control state,
                                slope;
        };





        public class Node : Entity
        {
            public static IDMap<Node> Collection = new IDMap<Node>();
            public class Side
            {
                public float[] Subdivision = new float[0];
                public Segment.EndPoint[] Segments = new Segment.EndPoint[1];

                public Node Parent { get; private set; }
                public bool IsOutbound { get; private set; }

                public Side(Node parent, bool isOutbound)
                {
                    Parent = parent;
                    IsOutbound = isOutbound;
                }

                public bool SlotAvailable(int index)
                {
                    return index < Segments.Length && Segments[index] == null;
                }

                public Connector BuildConnector(int subdiv, bool second_node)
                {
                    Connector result;
                    if (!BuildConnector(subdiv, second_node, out result))
                        throw new Exception("Internal state has become inconsistent");
                    return result;
                }

                public bool BuildConnector(int subdiv, bool second_node, out Connector result)
                {
                    if (!buildState(subdiv, second_node, out result.state))
                    {
                        result = new Connector();
                        return false;
                    }


                    if (IsOutbound ^ second_node)
                    {
                        result.slope = Parent.Slope;
                    }
                    else
                    {
                        result.slope = Parent.Slope * -1;
                    }

                    return true;
                }


                private bool buildState(int subdiv, bool second_node, out Control out_state)
                {
                    Debug.Assert(subdiv <= Subdivision.Length);

                    if (subdiv >= Segments.Length)
                    {
                        out_state = new Control();
                        return false;
                    }

                    Control mine = Parent.Position;
                    out_state = mine;
                    bool flip;
                    if (IsOutbound ^ second_node)
                    {
                        flip = false;
                        //out.slope = slopes[outbound][subdiv];
                    }
                    else
                    {
                        flip = true;
                        out_state.Direction *= -1;
                    }

                    if (Subdivision.Length == 0)
                        return subdiv == 0;
                    if (subdiv == 0)
                    {
                        out_state.Angle0 = mine.Angle0;
                        out_state.Angle1 = mine.Angle0 + (mine.Angle1 - mine.Angle0) * Subdivision[0];

                        out_state.Texcoord0 = mine.Texcoord0;
                        out_state.Texcoord1 = mine.Texcoord0 + (mine.Texcoord1 - mine.Texcoord0) * Subdivision[0];
                    }
                    else if (subdiv == Subdivision.Length)
                    {
                        out_state.Angle0 = mine.Angle0 + (mine.Angle1 - mine.Angle0) * Subdivision.Last();
                        out_state.Angle1 = mine.Angle1;

                        out_state.Texcoord0 = mine.Texcoord0 + (mine.Texcoord1 - mine.Texcoord0) * Subdivision.Last();
                        out_state.Texcoord1 = mine.Texcoord1;
                    }
                    else
                    {
                        out_state.Angle0 = mine.Angle0 + (mine.Angle1 - mine.Angle0) * Subdivision[subdiv - 1];
                        out_state.Angle1 = mine.Angle0 + (mine.Angle1 - mine.Angle0) * Subdivision[subdiv];

                        out_state.Texcoord0 = mine.Texcoord0 + (mine.Texcoord1 - mine.Texcoord0) * Subdivision[subdiv - 1];
                        out_state.Texcoord1 = mine.Texcoord0 + (mine.Texcoord1 - mine.Texcoord0) * Subdivision[subdiv];
                    }

                    if (flip)
                    {
                        out_state.Angle0 *= -1;
                        out_state.Angle1 *= -1;
                        out_state.Texcoord0 = 1.0f - out_state.Texcoord0;
                        out_state.Texcoord1 = 1.0f - out_state.Texcoord1;
                        Helper.Swap(ref out_state.Angle0, ref out_state.Angle1);
                        Helper.Swap(ref out_state.Texcoord0, ref out_state.Texcoord1);
                    }
                    return true;
                }

                internal void SumSlope(ref int summed, ref float distance, ref Control slope)
                {
                    Control local_state, remote_state, delta;

                    for (int i = 0; i < Segments.Length; i++)
                    {
                        Segment.EndPoint endPoint = Segments[i];
                        if (endPoint != null)
                        {
                            //Segment seg = endPoint.Parent;
                            Node.Side other = endPoint.Opposing.Connector.NodeSide;
                            Segment.Connector c = endPoint.Connector;

                            if (other != null)
                            {
                                if (buildState(i, false, out local_state)
                                    &&
                                    other.buildState(c.SubdivisionIndex, true, out remote_state))
                                {
                                    distance += Vec.Distance(local_state.Position, remote_state.Position);
                                    if (IsOutbound)
                                        delta = remote_state - local_state;
                                    else
                                        delta = local_state - remote_state;
                                    slope += delta;
                                    summed++;
                                }
                                else
                                    Debug.Fail("inconsistent state");
                            }
                        }
                    }
                }
            }
            public Control Position,
                                Slope;
            public Side InboundSide,
                                OutboundSide;
            public int LODIndex;

            public Node()
            {
                InboundSide = new Side(this, false);
                OutboundSide = new Side(this, true);

                Position = new Control();
                Position.Angle0 = -0.25f;
                Position.Angle1 = 0.25f;
                Position.Direction = Vec3.XAxis;
                Position.Scale = new Vec2(1);
                Position.Texcoord0 = 0;
                Position.Texcoord1 = 1;
                Position.Up = Vec3.ZAxis;

                Slope = new Control();
                LODIndex = 0;
            }

            public Side GetSide(bool outbound)
            {
                return outbound ? OutboundSide : InboundSide;
            }

            public bool SlotAvailable(bool outbound, int subdiv)
            {
                return GetSide(outbound).SlotAvailable(subdiv);
            }
            public int FirstAvailable(bool outbound)
            {
                Side side = GetSide(outbound);
                for (int i = 0; i < side.Segments.Length; i++)
                    if (side.Segments[i] == null)
                        return i;
                return -1;
            }
            public bool AnyAvailable(bool outbound) { return FirstAvailable(outbound) != -1; }
        }



        public class Segment : Entity
        {
            public static IDMap<Segment> Collection = new IDMap<Segment>();
            public struct Connector
            {
                public Node.Side NodeSide;
                public int SubdivisionIndex;	//!< Index of the specified node's subdivision
                public float ControlFactor;

                public Connector(Node.Side nodeSide, int subdivisionStep)
                {
                    NodeSide = nodeSide;
                    SubdivisionIndex = subdivisionStep;
                    ControlFactor = 0.3f;
                }

            }

            public class EndPoint
            {
                public Control Control;
                public Connector Connector;
                public Segment Parent;
                public EndPoint Opposing;

                public EndPoint(Segment parent, EndPoint opposing)
                {
                    Parent = parent;
                    Opposing = opposing;
                }
                internal void Unregister()
                {
                    Debug.Assert(Connector.NodeSide.Segments[Connector.SubdivisionIndex] == this);
                    Connector.NodeSide.Segments[Connector.SubdivisionIndex] = null;
                    Connector.NodeSide = null;
                }

                internal void ManagedUnlink()
                {
                    //Node n = Connector.Node;
                    //Debug.Assert(n != null);
                    Node.Side s = Connector.NodeSide;
                    Debug.Assert(Connector.SubdivisionIndex < s.Segments.Length);

                    s.Segments = Helper.EraseArrayElement(s.Segments, Connector.SubdivisionIndex);

                    for (int i = Connector.SubdivisionIndex; i < s.Segments.Length; i++)
                    {
                        EndPoint endPoint = s.Segments[i];
                        if (endPoint != null)
                            endPoint.Connector.SubdivisionIndex--;
                    }
                    s.Subdivision = Helper.EraseArrayElement(s.Subdivision, Connector.SubdivisionIndex);
                    //even(n->subdivision[c.outbound]);
                }
            }

            public EndPoint EndPoint0,
                             EndPoint1;

            public Control Control0,
                             Control1;

            public Segment()
            {
                EndPoint0 = new EndPoint(this, null);
                EndPoint1 = new EndPoint(this, EndPoint0);
                EndPoint0.Opposing = EndPoint1;
            }

            public EndPoint GetEndPoint(Node node)
            {
                if (EndPoint0.Connector.NodeSide.Parent == node)
                    return EndPoint0;
                if (EndPoint1.Connector.NodeSide.Parent == node)
                    return EndPoint1;
                throw new Exception("Integrity violated");
            }
        };



        public static class Helper
        {
            internal static T[] EraseArrayElement<T>(T[] array, int element)
            {
                T[] result = new T[array.Length - 1];
                Array.Copy(array, result, element);
                Array.Copy(array, element + 1, result, element, array.Length - element - 1);
                return result;
            }

            public static Segment Link(Segment.Connector slot0, Segment.Connector slot1, bool update_controls)
            {
                if (slot0.NodeSide == null || slot1.NodeSide == null || slot0.NodeSide.Parent == slot1.NodeSide.Parent)
                    return null;
                Node.Side n0 = slot0.NodeSide,
                            n1 = slot1.NodeSide;

                if (!n0.SlotAvailable(slot0.SubdivisionIndex)
                    ||
                    !n1.SlotAvailable(slot1.SubdivisionIndex))
                    return null;


                Segment seg = new Segment();
                seg.EndPoint0.Connector = slot0;
                seg.EndPoint1.Connector = slot1;
                Segment.Collection.InsertNew(seg);

                n0.Segments[slot0.SubdivisionIndex] = seg.EndPoint0;
                n1.Segments[slot1.SubdivisionIndex] = seg.EndPoint1;

                if (update_controls)
                    UpdateControls(seg);

                return seg;
            }

            public static Segment SmartLink(Node n0, Node n1, bool manage_slots, bool update_controls)
            {
                if (n0 == null || n1 == null || n0 == n1)
                    return null;
                Vec3 delta = n1.Position.Position - n0.Position.Position;

                bool outbound0 = Vec.Dot(n0.Position.Direction, delta) > 0.0f;
                if (!manage_slots && !n0.AnyAvailable(outbound0))
                {
                    outbound0 = !outbound0;
                    if (!n0.AnyAvailable(outbound0))
                        return null;
                }
                bool outbound1 = Vec.Dot(n1.Position.Direction, delta) < 0.0f;
                if (!manage_slots && !n1.AnyAvailable(outbound1))
                {
                    outbound1 = !outbound1;
                    if (!n1.AnyAvailable(outbound1))
                        return null;
                }

                Segment seg = new Segment();
                Segment.Collection.InsertNew(seg);
                //DBG_ASSERT_EQUAL__(segments.queryPointer(result),&seg);
                Node.Side s0 = n0.GetSide(outbound0);
                Node.Side s1 = n1.GetSide(outbound1);
                int subdiv0 = manage_slots ? smartMakeRoom(s0, n1) : n0.FirstAvailable(outbound0);
                //DBG_ASSERT_EQUAL__(segments.queryPointer(result),&seg);

                int subdiv1 = manage_slots ? smartMakeRoom(s1, n0) : n1.FirstAvailable(outbound1);

                s0.Segments[subdiv0] = seg.EndPoint0;
                s1.Segments[subdiv1] = seg.EndPoint1;
                seg.EndPoint0.Connector = new Segment.Connector(s0, subdiv0);
                seg.EndPoint1.Connector = new Segment.Connector(s1, subdiv1);

                for (int i = 0; i < s0.Segments.Length; i++)
                    if (s0.Segments[i] != null)
                        Debug.Assert(s0.Segments[i].Connector.SubdivisionIndex == i);
                for (int i = 0; i < s1.Segments.Length; i++)
                    if (s1.Segments[i] != null)
                        Debug.Assert(s1.Segments[i].Connector.SubdivisionIndex == i);

                if (update_controls)
                {
                    UpdateSlopes(n0, false);
                    UpdateSlopes(n1, false);
                    UpdateControls(seg);
                    for (int i = 0; i < s0.Segments.Length; i++)
                        if (s0.Segments[i].Parent != seg)
                            UpdateControls(s0.Segments[i].Parent);
                    for (int i = 0; i < s1.Segments.Length; i++)
                        if (s1.Segments[i].Parent != seg)
                            UpdateControls(s1.Segments[i].Parent);
                    Node.Side _s0 = n0.GetSide(!outbound0);
                    Node.Side _s1 = n1.GetSide(!outbound1);
                    for (int i = 0; i < _s0.Segments.Length; i++)
                        UpdateControls(_s0.Segments[i].Parent);
                    for (int i = 0; i < _s1.Segments.Length; i++)
                        UpdateControls(_s1.Segments[i].Parent);
                }
                return seg;
            }

            public static Segment FindSegment(Segment.Connector slot0, Segment.Connector slot1)
            {
                Node.Side n0 = slot0.NodeSide,
                            n1 = slot1.NodeSide;
                if (n0.Parent == n1.Parent || n0 == null || n1 == null)
                    return null;
                if (slot0.SubdivisionIndex >= n0.Segments.Length)
                    return null;
                if (slot1.SubdivisionIndex >= n1.Segments.Length)
                    return null;
                if (n0.Segments[slot0.SubdivisionIndex] == n1.Segments[slot1.SubdivisionIndex])
                    return n0.Segments[slot0.SubdivisionIndex].Parent;
                return null;

            }

            public static void Destroy(Segment seg)
            {
                if (seg == null)
                    return;
                seg.EndPoint0.Unregister();
                seg.EndPoint1.Unregister();
                Segment.Collection.Remove(seg);
            }

            public static void ManagedUnlink(Segment seg, bool update_controls)
            {
                if (seg == null)
                    return;

                Node.Side n0 = seg.EndPoint0.Connector.NodeSide;
                Node.Side n1 = seg.EndPoint1.Connector.NodeSide;
                seg.EndPoint0.ManagedUnlink();
                seg.EndPoint1.ManagedUnlink();
                Segment.Collection.Remove(seg);
                UpdateSlopes(n0.Parent, update_controls);
                UpdateSlopes(n1.Parent, update_controls);
            }

            public static void UpdateSlopes(Node node, bool update_connected_segments)
            {
                //static const count_t NumFloats = SurfaceDescription::TControl::NumFloats;

                float distance = 0;
                node.Slope = new Control();
                int summed = 0;
                node.InboundSide.SumSlope(ref summed, ref distance, ref node.Slope);
                node.OutboundSide.SumSlope(ref summed, ref distance, ref node.Slope);

                node.Slope /= (float)summed;
                node.Slope.Position = node.Position.Direction * distance / (float)summed;

                if (update_connected_segments)
                {
                    foreach (var endPoint in node.InboundSide.Segments)
                        if (endPoint != null)
                            UpdateControls(endPoint.Parent);
                    foreach (var endPoint in node.OutboundSide.Segments)
                        if (endPoint != null)
                            UpdateControls(endPoint.Parent);
                }
            }


            public static void BuildControlPoints(Connector begin, Connector end, out Control control_point0, out Control control_point1, float control_factor0/*=0.39*/, float control_factor1/*=0.39*/)
            {
                //Vec2	sd = new Vec2();

                //static const count_t	NumFloats = SurfaceDescription::TControl::NumFloats;
                //float	d[SurfaceDescription::TControl::NumFloats];
                //VecUnroll<NumFloats>::sub(end_points[1].state.field,end_points[0].data.field,d);
                //float len = VecUnroll<3>::length(d);

                //VecUnroll<3>::mad(end_points[0].data.field,end_points[0].slope.field,control_factor0*len,control_points[0].field);
                control_point0 = begin.state + begin.slope * control_factor0;
                //VecUnroll<NumFloats>::mad(begin.state.field,begin.slope.field,control_factor0,control_points[0].field);

                control_point1 = end.state - end.slope * control_factor1;
                //VecUnroll<NumFloats>::mad(end.state.field,end.slope.field,-control_factor1,control_points[1].field);
            }


            public static void UpdateControls(Segment segment)
            {
                Node.Side n0 = segment.EndPoint0.Connector.NodeSide,
                            n1 = segment.EndPoint1.Connector.NodeSide;
                Debug.Assert(n0 != null);
                Debug.Assert(n1 != null);
                Connector c0,
                            c1;
                c0 = n0.BuildConnector(segment.EndPoint0.Connector.SubdivisionIndex, false);
                c1 = n1.BuildConnector(segment.EndPoint1.Connector.SubdivisionIndex, true);

                BuildControlPoints(c0, c1, out segment.Control0, out segment.Control1, segment.EndPoint0.Connector.ControlFactor, segment.EndPoint1.Connector.ControlFactor);
            }

            public static void UpdateAllSlopes(bool update_all_controls)
            {
                Node.Collection.VisitElements(n =>
                {
                    UpdateSlopes(n, false);
                });

                if (update_all_controls)
                    Segment.Collection.VisitElements(s =>
                    {
                        UpdateControls(s);
                    });
            }

            private static void verifyIntegrity(Node.Side side)
            {
                if (side.Segments.Length > 0)
                    Debug.Assert(side.Subdivision.Length + 1 == side.Segments.Length);
                foreach (float sub in side.Subdivision)
                    Debug.Assert(sub >= 0 && sub <= 1);
                foreach (Segment.EndPoint endPoint in side.Segments)
                {
                    if (endPoint != null)
                    {
                        Segment seg = endPoint.Parent;
                        seg.GetEndPoint(side.Parent);//trigger verification
                    }
                }
            }

            private static void verifyIntegrity(Segment.EndPoint endPoint)
            {
                Node.Side n = endPoint.Connector.NodeSide;
                if (n == null)
                    throw new Exception("Targeted node does not exist");
                Console.Out.WriteLine("  Checking end point: node " + n + " " + (n.IsOutbound ? "outbound" : "inbound") + " subdiv " + endPoint.Connector.SubdivisionIndex);

                if (endPoint.Connector.SubdivisionIndex >= n.Segments.Length)
                    throw new Exception("Targeted subdivison " + endPoint.Connector.SubdivisionIndex + " points beyond node capacity " + n.Segments.Length);
                Console.Out.WriteLine("   Subdiv at " + (endPoint.Connector.SubdivisionIndex == 0 ? 0 : n.Subdivision[endPoint.Connector.SubdivisionIndex - 1]));
                Debug.Assert(n.Segments[endPoint.Connector.SubdivisionIndex] == endPoint);
            }

            public static void VerifyIntegrity()
            {
                Console.Out.WriteLine("Verifying node integrity");
                Node.Collection.VisitElements(n =>
                {
                    Console.Out.WriteLine("Node " + n.ID + ":");
                    verifyIntegrity(n.InboundSide);
                    verifyIntegrity(n.OutboundSide);
                });
                Console.Out.WriteLine("Verifying segment integrity");
                Segment.Collection.VisitElements(s =>
                {
                    Console.Out.WriteLine(" Segment " + s.ID + ": " + s.EndPoint0.Connector.NodeSide + " to " + s.EndPoint1.Connector.NodeSide);
                    verifyIntegrity(s.EndPoint0);
                    verifyIntegrity(s.EndPoint1);
                });

            }

            private static bool getSideOrientation(Node.Side side, out bool result)
            {
                for (int i = 0; i < side.Segments.Length; i++)
                    if (side.Segments[i] != null)
                    {
                        Segment seg = side.Segments[i].Parent;
                        bool second_node = seg.EndPoint1.Connector.NodeSide == side;
                        if (second_node ^ side.IsOutbound)
                        {
                            result = false;
                        }
                        result = true;
                        return true;
                    }
                result = false;
                return false;
            }

            public static bool NodeIsFlipped(Node node)
            {
                bool result;
                if (getSideOrientation(node.InboundSide, out result))
                    return result;
                if (getSideOrientation(node.OutboundSide, out result))
                    return result;
                return false;


            }

            public static void MakeEven(float[] subdivison_field)
            {
                for (int i = 0; i < subdivison_field.Length; i++)
                {
                    subdivison_field[i] = (float)(i + 1) / (float)(subdivison_field.Length + 1);
                }
            }

            struct SortEntry : IComparable
            {
                public Segment.EndPoint segment;
                //public Node remoteNode;
                public Vec3 remote_node_position;
                public float projected_x;

                public int CompareTo(object obj)
                {
                    SortEntry b = (SortEntry)obj;
                    return projected_x > b.projected_x ? 1 : (projected_x < b.projected_x ? -1 : 0);
                }
            }

            private static int smartMakeRoom(Node.Side nodeSide, Node opposing_node)
            {
                int need_slots = 1;
                foreach (var end in nodeSide.Segments)
                    need_slots += (end != null) ? 1 : 0;



                SortEntry[] sort = new SortEntry[need_slots];
                //SortEntry*out = sort.pointer();
                int at = 0;

                Node node = nodeSide.Parent;

                Vec3 x = Vec.Cross(node.Position.Direction, node.Position.Up);

                {
                    SortEntry entry = new SortEntry();
                    entry.remote_node_position = opposing_node.Position.Position;
                    Vec3 d = entry.remote_node_position - node.Position.Position;
                    entry.projected_x = Vec.Dot(d, x) / Vec.Dot(d, node.Position.Direction);
                    sort[at++] = entry;
                }




                if (need_slots > 1)
                {
                    foreach (var end in nodeSide.Segments)
                    {
                        if (end != null)
                        {
                            SortEntry entry = new SortEntry();
                            entry.segment = end;
                            //Segment seg = end.Parent;
                            //*segments.queryPointer(node.segments[outbound][i]);
                            //out->segment_end_index = seg.getEndIndex(node_index);
                            entry.remote_node_position = end.Opposing.Connector.NodeSide.Parent.Position.Position;
                            Vec3 d = entry.remote_node_position - node.Position.Position;
                            entry.projected_x = Vec.Dot(d, x) / Vec.Dot(d, node.Position.Direction);
                            sort[at++] = entry;
                        }
                    }
                }

                Debug.Assert(at == sort.Length);


                Array.Sort(sort);

                if (!nodeSide.IsOutbound)
                    Array.Reverse(sort);

                int result = -1;
                nodeSide.Segments = new Segment.EndPoint[sort.Length];

                nodeSide.Subdivision = new float[sort.Length - 1];
                MakeEven(nodeSide.Subdivision);

                //cout << "smart reorder of node "<<node_index<<" "<<(outbound?"outbound":"inbound")<<":"<<endl;
                for (int i = 0; i < sort.Length; i++)
                {

                    SortEntry entry = sort[i];
                    if (entry.segment != null)
                    {
                        //Segment seg = entry.segment.Parent;
                        entry.segment.Connector.SubdivisionIndex = i;
                        nodeSide.Segments[i] = entry.segment;
                        //cout << "mapped segment "<<entry.segment_index<<" ("<<seg.connector[0].node<<" to "<<seg.connector[1].node<<") connector["<<(int)entry.segment_end_index<<"] to slot "<<i<<endl;
                    }
                    else
                    {
                        Debug.Assert(result == -1);
                        nodeSide.Segments[i] = null;
                        result = i;
                        //cout << "mapped new segment to slot "<<i<<endl;
                    }
                }


                Debug.Assert(result != -1);

                return result;
            }



            internal static void Swap<T>(ref T p1, ref T p2)
            {
                T a = p1;
                p1 = p2;
                p2 = a;
            }
        }


    }

    namespace Bridge
    {
        public enum Theme
        {
            Glass,
            Wooden
        }

        public class Node : Structure
        {
            public Track.Control control;

            public void UpdateVariables()
            {
                Vec3Attributes["Position"] = control.Position;
                Vec3Attributes["Direction"] = control.Direction;
                Vec3Attributes["Up"] = control.Up;
                FloatAttributes["Scale.x"] = control.Scale.x;
                FloatAttributes["Scale.y"] = control.Scale.y;
                FloatAttributes["Angle[0]"] = control.Angle0;
                FloatAttributes["Angle[1]"] = control.Angle1;
                FloatAttributes["Texcoord[0]"] = control.Texcoord0;
                FloatAttributes["Texcoord[1]"] = control.Texcoord1;
                UpdateDescription();
            }

            public Node(Vec3 position, Vec3 direction, Vec2 scale) : base("Bridge.Node",new Box(position - scale.x, position + scale.x))
            {
                control.Position = position;
                control.Direction = direction;
                control.Scale = scale;
                control.Up = Vec3.ZAxis;
                control.Angle0 = -(float)Math.PI / 8.0f;
                control.Angle1 = (float)Math.PI / 8.0f;
                control.Texcoord0 = 0;
                control.Texcoord1 = 1;

                UpdateVariables();
            }
        }

        public class Segment : Structure
        {

            public Node From, To;
            public Bridge.Theme Theme = Bridge.Theme.Glass;
            public float FromControlFactor = 0.3f,
                            ToControlFactor = 0.3f;

            public void UpdateVariables()
            {
                IntAttributes["From.Node"] = (int)From.ID.Value;
                IntAttributes["To.Node"] = (int)To.ID.Value;
                FloatAttributes["From.ControlFactor"] = FromControlFactor;
                FloatAttributes["To.ControlFactor"] = ToControlFactor;
                if (Theme != 0)
                    IntAttributes["Theme"] = (int)Theme;
                else
                    IntAttributes.Unset("Theme");   //0 is implied, no need to transfer
                UpdateDescription();
            }

            public Segment(Node from, Node to) : base("Bridge.Segment", Box.Wrap(from.BoundingVolume,to.BoundingVolume))
            {
                To = to;
                From = from;
                UpdateVariables();
            }

            internal void OverrideNodeIndices(int nodeID0, int nodeID1)
            {
                IntAttributes["From.Node"] = nodeID0;
                IntAttributes["To.Node"] = nodeID1;
                UpdateDescription();
            }
        }

    }
   
}
