#!/usr/bin/env python
import rospy, sys
from blam_slam.srv import RemoveFactor
from pose_graph_visualizer.srv import HighlightEdge
from add_factor import yes_or_no

def connect(key_from, key_to):
    rospy.init_node('remove_factor_client')
    remove_factor = rospy.ServiceProxy('/husky/blam_slam/remove_factor', RemoveFactor)
    highlight_edge = rospy.ServiceProxy('/husky/pose_graph_visualizer/highlight_edge', HighlightEdge)
    response = highlight_edge(key_from, key_to, True)
    if response.success:
        if yes_or_no('The factor to be removed from the factor graph is now visualized in RViz.\nDo you confirm the removal?'):
            response = remove_factor(key_from, key_to, True)
            highlight_edge(key_from, key_to, False)  # remove edge visualization
            if response.success:
                print('Successfully removed a factor between %i and %i from the graph.' % (key_from, key_to))
            else:
                sys.exit('An error occurred while trying to remove a factor between %i and %i.' % (key_from, key_to))
        else:
            print('Aborted.')
            highlight_edge(key_from, key_to, False)  # remove edge visualization
    else:
        sys.exit('Error: The factor between keys %i and %i could not be visualized. Make sure the keys exist.' % (key_from, key_to))

if __name__ == '__main__':
    try:
        if len(sys.argv) < 3 or "h" in sys.argv[1].lower():
            print("""Usage:
    python {arg0} <from_key> <to_key>
        Removes an edge between keys from_key and to_key from the pose graph.
            """.format(arg0=sys.argv[0]))
        else:
            connect(int(sys.argv[1]), int(sys.argv[2]))

    except rospy.ROSInterruptException: pass
